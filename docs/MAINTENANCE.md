# Maintenance

## 1. Réglages des repos (une seule fois)

Dans `slayer-kit` et `slayer-kit-packs` :

1. **Settings → Actions → General → Workflow permissions** : **Read and write permissions**.
2. **Settings → Actions → General → Fork pull request workflows** : **Require approval for all external contributors**. Le code d'un inconnu ne tourne pas dans Actions sans ton accord.
3. **Settings → General → Features** : coche **Discussions** pour les questions d'utilisation.
4. **Settings → Code security** : active **Private vulnerability reporting**.

Personne d'autre que toi ne peut écrire dans les repos : les contributeurs passent par un fork et une pull request, que toi seul peux fusionner.

## 2. Activer la sauvegarde GitHub privée des utilisateurs (une seule fois)

1. Va sur github.com → **Settings → Developer settings → OAuth Apps → New OAuth App** et remplis :
   - Application name : `Slayer Kit`
   - Homepage URL : `https://github.com/ybyns2/slayer-kit`
   - Authorization callback URL : `https://github.com/ybyns2/slayer-kit`
2. Coche **Enable Device Flow**, puis enregistre.
3. Copie le **Client ID**, qui est public : ce n'est pas un secret. Ne crée **pas** de « client secret », l'appli n'en a pas besoin.
4. Colle-le dans `update.json` → `"githubClientId": "..."`, puis fais un commit sur `main`. Tout le monde l'a immédiatement, sans republier l'appli.

## 2 bis. Activer les alertes Twitch (une seule fois)

1. Active la **double authentification** sur ton compte Twitch : twitch.tv → Paramètres → Sécurité et confidentialité. Twitch l'exige pour créer une appli.
2. Va sur **dev.twitch.tv/console/apps → Enregistrer votre application** et remplis :
   - Nom : `Slayer Kit by yns_bnl`
   - URL de redirection : `http://localhost`
   - Catégorie : Broadcaster Suite
   - Type de client : **Publique**
3. Copie le **Client ID** dans `update.json` → `"twitchClientId": "..."`, puis fais un commit. Le bouton « Connecter Twitch » s'active pour tout le monde.

## 2 ter. Chat Kick

Le chat Kick est lu par l'appli (API publique `web.kick.com`). Aucun réglage à faire.

## 3. Publier une mise à jour

1. Dans le repo `slayer-kit`, va dans **Actions → Publier une mise à jour → Run workflow**.
2. Remplis le formulaire :
   - version (ex : `1.1.0`) ;
   - canal (`stable` ou `beta`) ;
   - critique (oui/non) ;
   - notes.
3. Le workflow enchaîne alors :
   - il compile ;
   - il crée l'installateur ;
   - il calcule l'empreinte SHA-256 ;
   - il crée la release ;
   - il met `update.json` à jour.

Au prochain lancement, tout le monde reçoit la notification. Une mise à jour **critique** s'installe sans demander, mais seulement quand la personne n'est pas en live.

## 4. Valider les contributions

### Code (`slayer-kit`)

1. Une pull request arrive : si c'est un nouveau contributeur, clique sur **Approve and run** pour lancer la compilation automatique.
2. Lis les changements dans l'onglet **Files changed**. Refuse tout ce qui envoie des données vers l'extérieur, touche aux clés ou télécharge depuis une source non vérifiée.
3. Si besoin, teste localement : `git fetch origin pull/NUMERO/head:pr-NUMERO`, puis `build-dev.cmd`.
4. Demande des changements (**Review changes → Request changes**) ou fusionne avec **Squash and merge**.
5. Rien n'est livré tant que tu ne publies pas une version (section 3).

### Serveurs (`slayer-kit-packs`)

Les contributeurs ouvrent soit une issue « Proposer un serveur » (sans Git), soit une pull request.

- Pour une issue, crée toi-même le dossier du pack à partir des fichiers joints.
- Pour une pull request, la vérification automatique contrôle le pack. Vérifie l'accord du staff et les visuels, puis fusionne.

Après la fusion, `index.json` est régénéré automatiquement et le serveur apparaît dans l'appli pour tout le monde.

## 5. Lien Discord d'aide

Mets-le dans `update.json` → `"helpDiscord": "https://discord.gg/..."`. Il est pris en compte sans republier l'appli.
