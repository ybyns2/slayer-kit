# Contribuer à Slayer Kit

Slayer Kit est ouvert aux contributions : correctifs, nouvelles fonctionnalités, traductions, documentation ou nouveaux serveurs RP. Toute proposition passe par une pull request relue et validée par le mainteneur ([@ybyns2](https://github.com/ybyns2)) avant d'arriver dans une version publiée.

## Par où commencer

| Tu veux… | Où aller |
|---|---|
| Signaler un bug | [Ouvrir une issue « Signaler un problème »](https://github.com/ybyns2/slayer-kit/issues/new?template=bug.yml) |
| Proposer une idée | [Ouvrir une issue « Proposer une idée »](https://github.com/ybyns2/slayer-kit/issues/new?template=idee.yml) |
| Ajouter un serveur RP | [slayer-kit-packs](https://github.com/ybyns2/slayer-kit-packs) |
| Modifier le code | Lis la suite |

Pour un changement important, ouvre d'abord une issue pour en discuter : ça évite de travailler sur quelque chose qui ne sera pas fusionné.

## Environnement

- Windows 10 ou 11
- Visual Studio 2022 Build Tools avec la charge de travail « Développement Desktop en C++ » et le SDK Windows 10/11
- CMake 3.24+ et Ninja (fournis avec les Build Tools)

```
build-dev.cmd       version de développement : sans droits admin, données dans des dossiers « SlayerKit-Dev »
build.cmd           version publique (manifeste requireAdministrator)
```

La version de développement utilise son propre mutex et le port local 8788 : elle peut tourner à côté de la version installée sans rien casser. Options disponibles dans cette version :

```
SlayerKit.exe --selftest        vérifie la fusion 3 voies des fichiers OBS
SlayerKit.exe --dash=N          ouvre directement l'onglet N du tableau de bord
SlayerKit.exe --step=N          ouvre directement l'étape N de l'assistant
```

## Organisation du code

```
src/
  app/        fenêtre, boucle de rendu, état de l'application
  core/       logique sans interface : OBS, audio, packs, mises à jour, sauvegardes, GitHub, serveur local
  pages/      écrans : assistant, tableau de bord, kit, live, pages web
  ui/         rendu Direct2D/DirectWrite et composants d'interface
kit/obs/      scripts Lua chargés dans OBS
installer/    script Inno Setup
tools/        scripts de publication
```

Règles de base :

- C++20, Win32 et Direct2D uniquement, pas de framework d'interface.
- La logique va dans `core/`, l'affichage dans `pages/` et `ui/`. Un module `core/` ne dépend jamais de `pages/`.
- Les opérations longues (réseau, disque, audio) passent par `Async` puis `Post` pour revenir sur le fil de l'interface.
- Les textes affichés sont en français, avec le tutoiement.
- Suis le style du fichier que tu modifies : 4 espaces, 120 colonnes, accolades sur la même ligne.

## Sécurité et vie privée

Ce sont des règles non négociables, une PR qui ne les respecte pas sera refusée :

- Les clés de stream et les jetons restent chiffrés par DPAPI sur le PC et ne sont jamais envoyés nulle part, ni écrits dans les logs, ni inclus dans la sauvegarde GitHub.
- Aucune télémétrie, aucune statistique envoyée.
- Tout téléchargement vérifie sa source et, quand c'est possible, son empreinte SHA-256.
- Les modifications des réglages FiveM ne se font que jeu fermé.

## Pull requests

1. Fork le repo et crée une branche à partir de `main` : `fix/clip-replay`, `feat/alertes-youtube`…
2. Fais des commits courts et clairs, au format [Conventional Commits](https://www.conventionalcommits.org/fr/) : `fix(obs): …`, `feat(audio): …`, `docs: …`.
3. Vérifie que `build.cmd` compile et teste ton changement avec `build-dev.cmd`.
4. Ouvre la pull request en remplissant le modèle.
5. La compilation automatique (GitHub Actions) doit passer. Le mainteneur relit, demande éventuellement des changements, puis fusionne.

Les changements fusionnés sont livrés à tout le monde à la version suivante, par la mise à jour automatique de l'application.

## Licence

En contribuant, tu acceptes que ton code soit publié sous la [licence MIT](LICENSE) du projet.
