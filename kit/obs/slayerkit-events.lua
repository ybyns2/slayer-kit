obs = obslua

local fichier = ""
local tampon = true

local function ecrire(enLive)
    if fichier == "" then return end
    local f = io.open(fichier, "w")
    if f == nil then return end
    f:write(string.format('{"live":%s,"time":%d}', enLive and "true" or "false", os.time()))
    f:close()
end

local function sur_evenement(evt)
    if evt == obs.OBS_FRONTEND_EVENT_STREAMING_STARTED then
        ecrire(true)
        if tampon and not obs.obs_frontend_replay_buffer_active() then obs.obs_frontend_replay_buffer_start() end
    elseif evt == obs.OBS_FRONTEND_EVENT_STREAMING_STOPPED then
        ecrire(false)
    elseif evt == obs.OBS_FRONTEND_EVENT_FINISHED_LOADING then
        if tampon and not obs.obs_frontend_replay_buffer_active() then obs.obs_frontend_replay_buffer_start() end
    elseif evt == obs.OBS_FRONTEND_EVENT_EXIT then
        ecrire(false)
    end
end

function script_description()
    return "<b>Slayer Kit &mdash; Suivi du live</b><br/>Signale a Slayer Kit le debut et la fin du live (annonce Discord, mode performance, statistiques) et lance le tampon de relecture pour les clips."
end

function script_properties()
    local p = obs.obs_properties_create()
    obs.obs_properties_add_text(p, "fichier", "Fichier d'etat", obs.OBS_TEXT_DEFAULT)
    obs.obs_properties_add_bool(p, "tampon", "Garder le tampon de relecture actif (clips a tout moment)")
    return p
end

function script_update(settings)
    fichier = obs.obs_data_get_string(settings, "fichier")
    tampon = obs.obs_data_get_bool(settings, "tampon")
end

function script_load(settings)
    obs.obs_frontend_add_event_callback(sur_evenement)
end
