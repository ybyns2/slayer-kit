obs = obslua

local scene_decompte = "2 \194\183 D\195\137COMPTE"
local scene_live     = "3 \194\183 EN LIVE"
local source_texte   = "D\195\169compte \194\183 chiffres"
local source_musique = "Musique d'attente"
local duree_min      = 5
local fondu_sec      = 6
local lancer_stream  = true

local restant  = 0
local en_cours = false
local affiche_plein = false
local volume_initial = nil
local actif = false

local function ecrire(txt)
    local src = obs.obs_get_source_by_name(source_texte)
    if src == nil then return end
    local s = obs.obs_data_create()
    obs.obs_data_set_string(s, "text", txt)
    obs.obs_source_update(src, s)
    obs.obs_data_release(s)
    obs.obs_source_release(src)
end

local function mmss(sec)
    if sec < 0 then sec = 0 end
    return string.format("%02d:%02d", math.floor(sec / 60), sec % 60)
end

local function volume_musique(v)
    local src = obs.obs_get_source_by_name(source_musique)
    if src == nil then return end
    if v == nil then
        if volume_initial ~= nil then obs.obs_source_set_volume(src, volume_initial) end
    else
        if volume_initial == nil then volume_initial = obs.obs_source_get_volume(src) end
        obs.obs_source_set_volume(src, volume_initial * v)
    end
    obs.obs_source_release(src)
end

local function nom_scene_active()
    local scene = obs.obs_frontend_get_current_scene()
    if scene == nil then return "" end
    local nom = obs.obs_source_get_name(scene)
    obs.obs_source_release(scene)
    return nom
end

local function reinitialiser()
    en_cours = false
    if not affiche_plein then
        volume_musique(nil)
        ecrire(mmss(duree_min * 60))
        affiche_plein = true
    end
end

local function passer_au_live()
    local src = obs.obs_get_source_by_name(scene_live)
    if src ~= nil then
        obs.obs_frontend_set_current_scene(src)
        obs.obs_source_release(src)
    end
end

local function seconde()
    if nom_scene_active() ~= scene_decompte then
        reinitialiser()
        return
    end

    if not en_cours then
        en_cours = true
        affiche_plein = false
        volume_musique(nil)
        restant = duree_min * 60
        ecrire(mmss(restant))
        return
    end

    restant = restant - 1
    if restant > 0 then
        ecrire(mmss(restant))
        if restant <= fondu_sec then volume_musique(restant / fondu_sec) end
        return
    end

    ecrire("C'EST PARTI !")
    if lancer_stream and not obs.obs_frontend_streaming_active() then
        obs.obs_frontend_streaming_start()
    end
    passer_au_live()
end

local function activer()
    if not actif then
        obs.timer_add(seconde, 1000)
        actif = true
    end
end

local function desactiver()
    if actif then
        obs.timer_remove(seconde)
        actif = false
    end
end

function script_description()
    return "<b>Slayer Kit &mdash; Decompte de debut de live</b><br/><br/>" ..
           "Affichez la scene de decompte : 5 minutes avec la musique, puis le stream " ..
           "demarre tout seul et la scene du jeu s'affiche.<br/>" ..
           "Quitter la scene avant la fin annule le decompte."
end

function script_properties()
    local p = obs.obs_properties_create()
    obs.obs_properties_add_text(p, "scene_decompte", "Scene du decompte",        obs.OBS_TEXT_DEFAULT)
    obs.obs_properties_add_text(p, "scene_live",     "Scene du jeu",             obs.OBS_TEXT_DEFAULT)
    obs.obs_properties_add_text(p, "source_texte",   "Source texte du decompte", obs.OBS_TEXT_DEFAULT)
    obs.obs_properties_add_text(p, "source_musique", "Source de la musique",     obs.OBS_TEXT_DEFAULT)
    obs.obs_properties_add_int (p, "duree_min",      "Duree (minutes)", 1, 60, 1)
    obs.obs_properties_add_int (p, "fondu_sec",      "Fondu de la musique (secondes)", 0, 30, 1)
    obs.obs_properties_add_bool(p, "lancer_stream",  "Lancer le stream a zero s'il n'est pas lance")
    return p
end

function script_defaults(settings)
    obs.obs_data_set_default_string(settings, "scene_decompte", scene_decompte)
    obs.obs_data_set_default_string(settings, "scene_live",     scene_live)
    obs.obs_data_set_default_string(settings, "source_texte",   source_texte)
    obs.obs_data_set_default_string(settings, "source_musique", source_musique)
    obs.obs_data_set_default_int   (settings, "duree_min",      duree_min)
    obs.obs_data_set_default_int   (settings, "fondu_sec",      fondu_sec)
    obs.obs_data_set_default_bool  (settings, "lancer_stream",  true)
end

function script_update(settings)
    volume_musique(nil)
    scene_decompte = obs.obs_data_get_string(settings, "scene_decompte")
    scene_live     = obs.obs_data_get_string(settings, "scene_live")
    source_texte   = obs.obs_data_get_string(settings, "source_texte")
    source_musique = obs.obs_data_get_string(settings, "source_musique")
    duree_min      = obs.obs_data_get_int   (settings, "duree_min")
    fondu_sec      = obs.obs_data_get_int   (settings, "fondu_sec")
    lancer_stream  = obs.obs_data_get_bool  (settings, "lancer_stream")
    volume_initial = nil
    en_cours = false
    affiche_plein = false
end

function script_load(settings)
    activer()
end

function script_unload()
    desactiver()
    volume_musique(nil)
end
