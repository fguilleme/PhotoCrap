# Notes de version

Les fonctions disponibles et leurs limites sont détaillées dans le [README](README.md),
le [guide Sélections](docs/SELECTIONS.md) et la [roadmap](ROADMAP.md).
Un [résumé en anglais](README.en.md) est également disponible.

## v0.23.0 — 12 novembre 2026

- Calques 3D éditables à partir de fichiers GLB, avec pose par calque, éclairage
  commun, manipulation sur le canevas et conservation du modèle dans le projet.
  Aperçu Metal sur Mac et rendu final en 8 ou 16 bits.
- Génération Meshy depuis du texte ou une image, récupération par identifiant
  de tâche, simplification optionnelle du maillage et bilan d’import des
  triangles, du fichier et des textures. [Limites GLB](docs/PROJECT_FORMAT.md#calques-3d).
- Qwen Edit propose **Sans LoRA** avec des réglages distincts pour le modèle de
  base. PhotoCrap réessaie une connexion initiale inaccessible avant l’envoi
  de la demande et réessaie une erreur d’authentification du trousseau macOS.
  [Guide IA](docs/AI_IMAGE_EDITING.md#qwen-edit-21-avec-api-gradio-v2).

## v0.22.0 — 11 novembre 2026

- Transformation manuelle (⌘T / Ctrl+T) avec poignées, pivot et champs numériques.
  Échelle, rotation, inclinaison, distorsion, perspective et déformation par
  grille. Transformation du contour seul, répétition avec ou sans copie et
  aperçu avant validation en 8/16 bits. [Guide](docs/TRANSFORM.md).

- La conversion d’une sélection en masque la désélectionne dans la même étape
  d’historique, pour ne plus tronquer le flou au bord du masque. Une sélection
  volontaire peut être conservée et sa portée contrôlée par « Limiter à la sélection ».

- Réglages et filtres directement sur le calque de pixels ou son masque de fusion,
  avec aperçu réduit, Annuler/Appliquer, sélection et historique. Le flou sur masque
  conserve les pixels couleur du calque.
- Masques vectoriels depuis la Plume, ancres et poignées éditables, vignette distincte,
  densité et contour progressif modifiables, coexistence avec le masque peint,
  transformations, historique et persistance. [Guide](docs/MASK_ADJUSTMENTS.md).

- Fiabiliser les connexions Qwen entre générations et reprendre une lecture SSE
  interrompue sur le même identifiant. Une coupure pendant l’envoi est signalée
  sans resoumettre automatiquement la génération, y compris sous Qt 6.8.

- Fournisseur Qwen Edit 2.1 par API Gradio JSON v2 : image/sélection, flux SSE,
  authentification HTTP Basic avec mot de passe dans le trousseau, résultat sur
  calque ou nouveau document, sans ComfyUI. Liste des 19 adaptateurs LoRA,
  graine/aléatoire, guidance et étapes modifiables par profil ou génération ;
  graine réellement renvoyée et paramètres conservés dans les informations du calque.

## v0.21.1 — 9 novembre 2026

- Conserver la pipette temporaire tant qu’Option/Alt est maintenue, même lorsqu’un
  ancien événement de survol arrive après la touche ; restaurer le pinceau au
  relâchement et réinitialiser les modificateurs à l’ouverture d’un dialogue.
- Atténuer le guide de la Plume sur les contours peints très fins à faible zoom,
  sans modifier les pixels ni les poignées ; tester le trait pointillé séparément
  sur le rendu logiciel en complément du parcours natif macOS.

## v0.21.0 — 9 novembre 2026

- Documents multiples en onglets : calques, masques, sélection, tracés et historique
  indépendants ; zoom, défilement et rotation restaurés, raccourcis configurables.
  Ouvrir, créer et déposer des fichiers conserve les documents déjà ouverts.
- Fermeture d’un onglet et revue de tous les documents modifiés avant Quitter,
  avec sauvegarde, abandon et annulation ; demandes IA attachées à leur source.
- Destination IA « Nouveau document · résolution native », proposée par défaut
  pour les upscales ; résultat non rééchantillonné vers l’ancien canevas, masque
  redimensionné et source inchangée. Résultats en arrière-plan sans vol de focus.

- Réutiliser une clé API enregistrée depuis un unique profil du même service et
  du même endpoint, notamment de fal.ai édition vers upscale ; conserver les
  clés OpenAI séparées et les clés propres au profil prioritaires. Corriger la
  priorité du trousseau sur l’environnement après redémarrage, les adresses
  équivalentes et les indicateurs de sauvegarde entre instances. Afficher la
  provenance de la clé et distinguer une saisie encore non enregistrée.
- Retirer les outils dédiés d’ajout/suppression d’ancres ; conserver ces actions
  dans la Plume avec Ajout/Suppression auto et migrer les anciennes dispositions.
  Distinguer l’astérisque de nouveau tracé des signes +/− de modification.
- Convertir le tracé sélectionné en une seule entrée de forme modifiable, avec
  couleurs et remplissage liés aux poignées, Annuler/Rétablir et projets 8/16 bits.
  Distinguer les tracés simples des formes dans le panneau, proposer Créer une
  forme et nommer explicitement le remplissage peint dans les pixels.

## v0.20.0 — 9 novembre 2026

- Activer Espace + glisser pour déplacer le canevas dès le premier geste, même
  quand le focus est resté hors du canevas après l’accueil. Afficher la main
  ouverte/fermée et restaurer le pinceau au relâchement, sans modifier l’image ;
  préserver Espace dans les champs de saisie et les dialogues.

- Ajouter le preset fal.ai Real-ESRGAN : modèle photo x2plus, ×2, sans prompt,
  sortie PNG et restauration des visages désactivée ; choix du modèle et du
  facteur, source IA native, masque et placement conservés comme pour Topaz.

- Réparer automatiquement les profils Topaz créés manuellement : image unique
  via `image_url`, prompt supprimé et configuration héritée normalisée, sans
  changer le nom ni la référence de clé. Afficher les détails de validation
  HTTP 422 au lieu du seul message réseau, sans recopier l’image envoyée.

- Traiter des images sans prompt avec des profils fal.ai/ComfyUI configurables ;
  preset Topaz Upscale Precision, choix du modèle et du facteur d’agrandissement.
  Reprendre la source IA originale du calque pour l’upscale, préserver son masque
  et son placement ; conserver les dimensions du document et l’annulation.
  Réponses fal.ai singulières prises en charge et limites de sortie vérifiées
  avant envoi. La création d’un nouveau document attend le mode multidocument.

- Ouvrir une image par dépôt sur la zone principale ou l’accueil, en conservant
  la protection des modifications non enregistrées. Déclarer les images pour
  « Ouvrir avec PhotoCrap » sur macOS et transmettre le fichier depuis le lanceur
  Linux. Le dépôt dans la pile conserve son comportement d’import comme calque.

- Importer une ou plusieurs images avec **+ → Importer une image…**, ou les
  déposer entre les calques avec une ligne d’insertion et défilement aux bords.
  Chargement asynchrone, RAW et précision 8/16 bits conservés ; lot atomique
  annulable en une étape. Les images sont centrées sans changer le document.

## v0.19.0 — 8 novembre 2026

- Retirer le bandeau de marque et le nom d’outil au-dessus des réglages ; masquer
  la barre générique lorsqu’elle est vide et compacter les autres outils à 44 px.
  Annuler, Rétablir et Exporter restent accessibles dans la barre du document.
- Ne plus lire les clés IA au démarrage ou à l’ouverture des réglages : lecture
  du seul fournisseur choisi au lancement d’une génération, requêtes simultanées
  regroupées et clé réutilisée pendant la session. Aucun nouvel essai automatique
  après refus d’accès au trousseau ; annulation et changements de profil respectés.
- Nouveau conteneur de projets binaire v14 : tuiles natives compressées sans perte
  avec Zstandard, index CBOR, fonds uniformes et données identiques dédupliquées.
  Plus de PNG/Base64 pour les pixels édités ; sources IA originales conservées
  comme blocs binaires. Échantillons 8/16 bits exacts, sauvegarde atomique et
  limite de fichier de 8 Gio. Lecture des anciens JSON v1–v13 conservée ; les
  anciennes applications ne peuvent pas ouvrir les nouvelles sauvegardes.

- Aperçus Quick Look et miniatures Finder pour les projets `.pcr` sur macOS,
  via des extensions natives indépendantes de Qt. Aperçu compact de composition
  intégré aux nouvelles sauvegardes et réutilisé par les fichiers récents.
- Repli identifié sur une image source pour les anciens projets ; ouverture
  des documents depuis le Finder et le bouton Quick Look de macOS.

## v0.18.2 — 8 novembre 2026

Cette version reprend les fonctions et correctifs de v0.18.0/v0.18.1 ci-dessous.
Les comparaisons des rendus de bruit, mosaïque et filtres spatiaux attendent
désormais la fin réelle du rendu complet asynchrone, au lieu de capturer
un ancien aperçu après un délai fixe. Les tolérances de comparaison sont inchangées.

## v0.18.1 — 8 novembre 2026, publication ignorée

Le [workflow du tag](https://github.com/fguilleme/PhotoCrap/actions/runs/42428235039)
a validé les tests et le paquet Linux. Deux captures de rendu ont encore échoué
sur macOS avant l’arrivée de l’image complète ; la publication a été bloquée.

Cette version reprend toutes les nouveautés de v0.18.0 ci-dessous.

- Attendre la fin de l’ouverture du panneau Pinceau et son dessin initial avant
  de vérifier qu’un changement de lissage ne provoque pas de nouveau dessin.
- Produire un vrai geste de sortie/entrée sur une poignée de dégradé, puis
  attendre le curseur prévu, au lieu d’un déplacement synthétique à la même place.
- Attendre le dessin des poignées de la Plume avant de contrôler leur moteur de rendu.
- Comparer les sorties IA mises à l’échelle à une image de référence Qt native
  complète, en conservant des comparaisons exactes en 8 et 16 bits sur chaque architecture.

## v0.18.0 — 8 novembre 2026, publication ignorée

Le [workflow du tag](https://github.com/fguilleme/PhotoCrap/actions/runs/42426564928)
a bloqué la publication après deux échecs de synchronisation des contrôles
d’interface sur macOS/Qt 6.8.3 et trois références de couleur IA trop strictes
vis-à-vis des arrondis natifs sous Linux. Les tests IA du runner macOS ont passé.

- Créer des calques depuis la composition ou une sélection avec OpenAI GPT Image,
  fal.ai, ComfyUI ou une API d’édition compatible. Conserver plusieurs fournisseurs,
  les clés dans le trousseau système et une bibliothèque de prompts. Générer jusqu’à
  huit variantes depuis la même source, avec suivi visible et annulation.
- Identifier chaque calque IA par fournisseur, modèle et numéro/total ; conserver
  le prompt et la date dans le projet. Empêcher la double insertion d’une variante.
  OpenAI et fal.ai ont été essayés dans l’application ; ComfyUI reste à valider
  avec un serveur et un workflow réels. [Guide IA](docs/AI_IMAGE_EDITING.md)
- Stocker les sources IA intactes à leur résolution d’origine dans les projets
  v13, pour éviter les PNG 16 bits agrandis inutilement. Conserver les masques et
  revenir au raster natif après retouche des pixels. Les anciens calques IA sans
  source originale conservent leur stockage existant.
- Ajouter les dégradés dynamiques éditables sur le canevas, avec poignées, points
  de couleur et aperçu GPU 8/16 bits ; compléter les options du Pot de peinture.
- Étendre les outils d’édition aux masques et permettre de régler les valeurs
  numériques en glissant leur libellé.
- Couper, copier et coller les sélections en 8/16 bits ; ajouter les fusions de
  calques/groupes/calques visibles, copies fusionnées et aplatissement.
- Réutiliser les ressources des piles GPU : textures uniformes, réductions,
  signatures de calques, paramètres tonals et cibles de filtres/masques.

## v0.17.2

- Vérifier les quatre empreintes raster qui varient entre runners x86-64 à
  l’aide des deux références exactes observées. Les tests d’équivalence entre
  composition régionale et intégrale continuent de comparer chaque pixel.

## v0.17.1 — publication ignorée

Le workflow a validé Ubuntu et macOS. Le runner Windows a validé l’équivalence
de composition et signalé quatre empreintes raster différentes du premier
runner Windows. [Workflow du tag](https://github.com/fguilleme/PhotoCrap/actions/runs/42130216592)

- Stabiliser les arrondis de conversion alpha dans les modes de fusion 16 bits
  pour obtenir les mêmes pixels en composition régionale et intégrale.
- Synchroniser les contrôles de survol du lasso polygonal et du recadrage avec
  les événements et le rendu QML.
- Vérifier les empreintes exactes du moteur avec les références des backends
  raster utilisés sur les plateformes de validation.

## v0.17 — tag du 5 novembre 2026, publication ignorée

Le tag `v0.17` correspond à la version interne `0.17.0`. GitHub Actions a
ignoré sa publication à la suite des échecs de tests.
[Validation du tag](docs/VALIDATION.md#validation-du-tag-v017)

- Étendre la retouche native au 16 bits et ajouter les calques de forme
  éditables de la Plume, la conversion d’un tracé en forme et le panneau Tracés.
- Ajouter les styles, l’anticrénelage et le contour progressif aux sélections
  géométriques. Revoir la charge, le mélange et le prélèvement multicalque du pinceau mélangeur.
- Étendre l’aperçu GPU aux réglages tonals, aux masques progressifs et aux
  combinaisons éligibles de fusion, de bruit et de mosaïque. Mutualiser les
  ressources des tuiles, des filtres spatiaux et des niveaux de zoom.
  [Rendu GPU](docs/GPU_RENDERING.md)
- Conserver les cadres et poignées de tranches sur GPU, réutiliser les petits
  tampons des numéros au défilement et libérer le Canvas de repli au retour GPU.
- Réduire les allocations lors de la lecture et de l’écriture des projets, des
  retouches régionales et des grands exports PNG, BMP, JPEG et WebP.
  [Plan d’optimisation](docs/OPTIMIZATION_PLAN.md)

## v0.16 — 2 novembre 2026

Tag Git : `v0.16`. Version de l’application et des paquets : `0.16.0`.
Les trois plateformes sont validées dans le [workflow du tag](https://github.com/fguilleme/PhotoCrap/actions/runs/41430300211).

- Réutiliser les textures visibles et mettre à jour les tuiles modifiées du canevas.
- Accélérer les aperçus des réglages, masques et modes de fusion éligibles sur GPU,
  avec repli CPU et rendu final natif 8/16 bits. Voir les conditions dans le
  [guide du rendu GPU](docs/GPU_RENDERING.md).
- Animer les contours de sélection et afficher les guides d’outils avec le graphe
  de scène ; conserver les graphiques des panneaux et limiter leurs recalculs.
- Ajouter les calques de texte éditables et la création automatique de calques
  pour les outils concernés ; voir le [guide Texte](docs/TEXT_TOOL.md).
- Prévisualiser les presets au survol et corriger le zoom Ctrl-molette sous Linux.
- Empêcher le proxy du Navigateur d’écraser une demande de contour progressif
  du masque dans la file de rendu GPU.
- Migrer la CI vers `macos-15` après le retrait de `macos-14` et conserver les
  diagnostics des tests en échec.

## v0.15 — 29 octobre 2026

Tag Git : `v0.15`. Version de l’application et des paquets : `0.15.0`.

### Calques et mémoire

- Réduire les copies des sources et du composite pendant les rendus en mode Normal,
  ainsi que la copie pleine résolution au retour du worker.
- Écarter les calques invisibles, les groupes masqués et les calques d’opacité nulle
  de la préparation des proxies, en conservant la précision du rendu complet.
- Réutiliser les bandes hautes encore présentes dans le LRU sans reconstruire leur
  bande basse évincée. Le cache RAM de fréquences reste limité à 128 Mio.
- Vérifier le rendu de 64 calques remplis en 8/16 bits, avec masques, opacités,
  retouche locale et Annuler/Rétablir. Clarifier la portée du test des 301 calques,
  qui valide le stockage compact et le rechargement.

### Cache de travail disque

- Stocker les gros rendus de fréquences dans des blocs natifs 8/16 bits sans
  recompression, avec SHA-256, publication atomique et lectures mappées en lecture seule.
- Réutiliser les blocs entre workers avant de recalculer ; conserver les blocs encore
  utilisés après leur éviction de l’index et nettoyer les sessions abandonnées.
- Ajouter Réglages → Cache de travail : dossier configurable, budget de 0 à 64 Gio
  (4 Gio par défaut), réserve de 512 Mio d’espace libre, statistiques et vidage.
  Zéro désactive le cache. Un stockage indisponible laisse le rendu fonctionner.
- Garder les petits aperçus et les demandes de vignettes de l’interface en RAM.
  Les bandes préparées pour la retouche deviennent des pixels éditables indépendants
  des fichiers temporaires.

### Couvertures de traits par tuiles

- Allouer uniquement les tuiles de 256 × 256 touchées par le pinceau, le crayon ou
  la gomme, sur les pixels et les masques, avec débit cumulatif et précision 8/16 bits.
- Réduire la couverture d’un petit trait sur un masque de document 24 Mpx/16 bits
  à 512 Kio, au lieu d’environ 183 Mio pour le plan complet.
- Introduire le partage de versions de tuiles et leur stockage disque avec références
  indépendantes de l’index du cache, pour préparer la migration des plans de calques.
- Étendre les tests aux frontières des tuiles, à la concurrence, aux erreurs disque,
  à l’annulation du geste et à Annuler/Rétablir.

### Compatibilité et limites

- Format `.pcr` v8 inchangé, lecture des versions 1 à 7 ; fonctionnalités de v0.14 conservées.
- Les plans éditables des calques et l’historique restent des images complètes en RAM.
  Le stockage par tuiles de ces plans n’est pas encore disponible.
- Limites inchangées : 32 Mpx, 512 calques et 2 Gio de plans de calques.
  Le cache de travail temporaire ne tient pas lieu d’autosauvegarde.

## v0.14 — 28 octobre 2026

Tag Git : `v0.14`. Version de l’application et des paquets : `0.14.0`.

### Sélections

- Plage de couleurs avec prélèvement sur le canevas ou l’aperçu sans fermer le dialogue,
  ajout/retrait d’échantillons, tolérance, portée localisée et inversion.
- Option Zones contiguës pour restreindre la sélection aux régions reliées aux échantillons ;
  aperçus image/masque synchronisés avec zoom à résolution native et déplacement.
- Contour progressif (Maj+F6), rayon de 1 à 1 000 px, avec couverture partielle conservée
  par la peinture, les remplissages, la copie, les déplacements et les masques.
- Frange, Lisser, Dilater et Contracter : calcul régional asynchrone et annulable,
  avec rejet des résultats devenus obsolètes.
- Annulation, rétablissement et resélection sans rendre un projet sauvegardé « modifié »
  lorsque seuls les contours changent. Les couvertures sont conservées dans l’historique.
- Affichage du contour de sélection indépendant de la sélection active ; sélection des
  calques similaires par nature et commandes supplémentaires configurables.
- Catalogue de 298 raccourcis et gestes, dont 121 NYI désactivés et documentés.

### Dialogues de fichiers

- Correction du contraste des dialogues Qt Quick de repli sous Linux : en-têtes,
  chemin, liste de fichiers, ligne sélectionnée et boutons restent lisibles en thème sombre.
- Repli adapté à Qt 6.8 ; conservation des dialogues natifs lorsqu’ils sont disponibles.
- Test `file_dialog_fallback` couvrant ouverture, enregistrement, export, choix de dossier,
  contraste et ouverture effective d’une image, dans la matrice CI des trois plateformes.

### Compatibilité et limites

- Format `.pcr` version 8 conservé, avec lecture des versions 1 à 7. Les sélections et
  leur historique restent temporaires ; elles ne sont pas enregistrées dans les projets.
- Les pixels de travail restent en 8 ou 16 bits par canal ; la couverture d’une sélection
  progressive utilise un masque distinct à 256 niveaux.
- Sujet, Ciel, Sélectionner et masquer, Mode Masque, transformation de sélection et couches
  alpha nommées restent NYI. Les algorithmes ne garantissent pas un rendu identique à Adobe.
- DMG macOS arm64, AppImage Linux x86_64, installateur et ZIP Windows x64, avec SHA-256 ;
  publication automatique après réussite de tous les contrôles de la matrice GitHub Actions.

## v0.13.1

- Plume Bézier et édition de points, sauvegarde des tracés dans les projets v8,
  conversion en sélection sans poignées superposées, remplissage et contour raster.
- Gomme magique asynchrone, annulable et compatible avec les pixels 8/16 bits.
- Menus contextuels distincts pour les calques et leurs masques.
- Fermeture de l’application avec des panneaux flottants ou masqués.
- Correction des curseurs natifs macOS ; v0.13.1 remplace le tag de préparation
  v0.13.0, qui n’avait pas publié de paquets.

Les fonctions antérieures sont recensées dans la section
[Disponible dans v0.12.0](ROADMAP.md#disponible-dans-v0120) de la roadmap.
