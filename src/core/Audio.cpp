#include "core/Audio.h"

#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <thread>

using Microsoft::WRL::ComPtr;

namespace sk::audio {

namespace {

float ToDb(double v) { return v <= 1e-9 ? -100.f : (float)(20.0 * std::log10(v)); }

ComPtr<IMMDeviceEnumerator> Enumerator() {
    ComPtr<IMMDeviceEnumerator> en;
    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&en));
    return en;
}

std::vector<Device> List(EDataFlow flow) {
    std::vector<Device> out;
    auto en = Enumerator();
    if (!en) return out;
    std::wstring defId;
    ComPtr<IMMDevice> def;
    if (SUCCEEDED(en->GetDefaultAudioEndpoint(flow, eConsole, &def))) {
        LPWSTR id = nullptr;
        def->GetId(&id);
        if (id) defId = id;
        CoTaskMemFree(id);
    }
    ComPtr<IMMDeviceCollection> coll;
    if (FAILED(en->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &coll))) return out;
    UINT n = 0;
    coll->GetCount(&n);
    for (UINT i = 0; i < n; ++i) {
        ComPtr<IMMDevice> d;
        coll->Item(i, &d);
        LPWSTR id = nullptr;
        d->GetId(&id);
        Device dev;
        dev.id = id ? id : L"";
        CoTaskMemFree(id);
        ComPtr<IPropertyStore> props;
        if (SUCCEEDED(d->OpenPropertyStore(STGM_READ, &props))) {
            PROPVARIANT v;
            PropVariantInit(&v);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &v)) && v.vt == VT_LPWSTR) dev.name = v.pwszVal;
            PropVariantClear(&v);
        }
        dev.isDefault = dev.id == defId;
        dev.capture = flow == eCapture;
        out.push_back(dev);
    }
    auto virt = [](const Device& d) {
        for (auto w : {L"Voicemeeter", L"VoiceMeeter", L"VB-Audio", L"CABLE", L"Voicemod", L"Virtual"})
            if (d.name.find(w) != std::wstring::npos) return true;
        return false;
    };
    std::stable_sort(out.begin(), out.end(), [&](auto& a, auto& b) {
        if (a.isDefault != b.isDefault) return a.isDefault;
        return !virt(a) && virt(b);
    });
    return out;
}

ComPtr<IMMDevice> Open(const std::wstring& id) {
    auto en = Enumerator();
    ComPtr<IMMDevice> d;
    if (!en) return d;
    if (id.empty() || id == L"default") en->GetDefaultAudioEndpoint(eCapture, eConsole, &d);
    else en->GetDevice(id.c_str(), &d);
    return d;
}

}

std::vector<Device> Inputs() { return List(eCapture); }
std::vector<Device> Outputs() { return List(eRender); }

float PeakLevel(const std::wstring& deviceId) {
    auto d = Open(deviceId);
    if (!d) return 0;
    ComPtr<IAudioMeterInformation> meter;
    if (FAILED(d->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, nullptr, &meter))) return 0;
    float p = 0;
    meter->GetPeakValue(&p);
    return p;
}

bool Record(const std::wstring& deviceId, float seconds, Phase* out, const TestStep& step, int phaseIndex,
            const std::atomic<bool>& cancel) {
    auto d = Open(deviceId);
    if (!d) return false;
    ComPtr<IAudioClient> client;
    if (FAILED(d->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client))) return false;
    WAVEFORMATEX* fmt = nullptr;
    if (FAILED(client->GetMixFormat(&fmt))) return false;
    bool isFloat = fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                   (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                    reinterpret_cast<WAVEFORMATEXTENSIBLE*>(fmt)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    int channels = fmt->nChannels, bits = fmt->wBitsPerSample;
    DWORD rate = fmt->nSamplesPerSec;
    HRESULT hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, fmt, nullptr);
    CoTaskMemFree(fmt);
    if (FAILED(hr)) return false;
    ComPtr<IAudioCaptureClient> cap;
    if (FAILED(client->GetService(IID_PPV_ARGS(&cap)))) return false;
    client->Start();

    const size_t window = rate / 20;
    std::vector<float> windowsDb;
    double acc = 0;
    size_t accN = 0;
    double peak = 0;
    size_t total = 0, wanted = (size_t)(seconds * rate);
    while (total < wanted && !cancel) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        UINT32 packet = 0;
        while (SUCCEEDED(cap->GetNextPacketSize(&packet)) && packet) {
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            if (FAILED(cap->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) break;
            for (UINT32 f = 0; f < frames; ++f) {
                double s = 0;
                if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                    for (int c = 0; c < channels; ++c) {
                        if (isFloat && bits == 32) s += reinterpret_cast<float*>(data)[f * channels + c];
                        else if (bits == 16) s += reinterpret_cast<short*>(data)[f * channels + c] / 32768.0;
                        else if (bits == 32) s += reinterpret_cast<int*>(data)[f * channels + c] / 2147483648.0;
                    }
                    s /= channels;
                }
                peak = std::max(peak, std::fabs(s));
                acc += s * s;
                if (++accN >= window) {
                    windowsDb.push_back(ToDb(std::sqrt(acc / accN)));
                    acc = 0;
                    accN = 0;
                    if (step) step(phaseIndex, (float)total / wanted, windowsDb.back());
                }
            }
            total += frames;
            cap->ReleaseBuffer(frames);
        }
    }
    client->Stop();
    if (cancel || windowsDb.empty()) return false;

    std::sort(windowsDb.begin(), windowsDb.end());
    if (phaseIndex == 0) {
        out->rmsDb = windowsDb[windowsDb.size() / 2];
    } else {
        size_t from = windowsDb.size() * 6 / 10;
        double sum = 0;
        for (size_t i = from; i < windowsDb.size(); ++i) sum += windowsDb[i];
        out->rmsDb = (float)(sum / (windowsDb.size() - from));
    }
    out->peakDb = ToDb(peak);
    return true;
}

TestResult Analyse(const Phase& silence, const Phase& voice, const Phase& shout) {
    TestResult r;
    r.ok = true;
    r.silence = silence;
    r.voice = voice;
    r.shout = shout;
    float noise = silence.rmsDb, speech = voice.rmsDb;
    if (speech - noise < 6) {
        r.ok = false;
        r.error = "Je n'ai presque pas entendu ta voix : vérifie que c'est le bon micro et qu'il n'est pas coupé.";
        return r;
    }
    float gain = std::clamp(-20.f - speech, -10.f, 20.f);
    gain = std::min(gain, -1.f - shout.peakDb + 8.f);
    r.gainDb = std::round(gain * 2) / 2;

    float noiseAfter = noise + r.gainDb;
    float speechAfter = speech + r.gainDb;
    r.rnnoise = noiseAfter > -70;
    float residual = noiseAfter - (r.rnnoise ? 15.f : 0.f);
    r.gateOpenDb = std::round(std::clamp(residual + 12.f, -60.f, speechAfter - 10.f));
    r.gateCloseDb = r.gateOpenDb - 8;
    r.compThresholdDb = std::round(std::clamp(speechAfter + 4.f, -30.f, -8.f));
    r.compRatio = shout.peakDb - voice.rmsDb > 24 ? 6.f : 4.f;

    float snr = speech - noise;
    r.quality = snr >= 40 ? 3 : snr >= 30 ? 2 : snr >= 20 ? 1 : 0;
    const wchar_t* q[] = {L"Micro faible ou bruyant : le kit compense au maximum (suppression de bruit + portillon serré).",
                          L"Micro correct : un peu de bruit de fond, corrigé par la suppression de bruit.",
                          L"Bon micro : voix claire, réglages légers.",
                          L"Excellent micro : voix très propre, presque rien à corriger."};
    r.verdict = q[r.quality];
    if (speech < -45) r.verdict += L" Ta voix arrive très faible : le gain a été monté.";
    if (shout.peakDb > -1) r.verdict += L" Tes cris saturent : le limiteur protège le live.";
    return r;
}

}
