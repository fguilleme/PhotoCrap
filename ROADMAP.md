# Roadmap PhotoCrap

État au **10 novembre 2026**. **v0.21.1** comprend les documents multiples, les résultats
IA à résolution native dans un nouvel onglet, la réutilisation des clés entre profils
du même service et une Plume simplifiée avec formes modifiables. Le format binaire
v14, Quick Look et les imports par dépôt restent disponibles.
Le correctif stabilise la pipette Option/Alt et les contours fins sous le guide de la Plume.
[Notes de version](CHANGELOG.md) ·
[Dernière version publiée](https://github.com/fguilleme/PhotoCrap/releases/latest)

La [release v0.16](https://github.com/fguilleme/PhotoCrap/releases/tag/v0.16) fournit les paquets macOS arm64, Linux x86_64 et Windows x64 en version **0.16.0**, avec SHA-256. Le [workflow du tag](https://github.com/fguilleme/PhotoCrap/actions/runs/41430300211) a validé les builds, tests, déploiements et paquets des trois plateformes avant publication. Voir les [notes de version](CHANGELOG.md).

## En préparation

- [x] Qwen Edit 2.1 via Gradio v2 : HTTP Basic sécurisé, événements SSE, 19 LoRA,
  paramètres de génération et résultat sur un calque ou dans un nouveau document.
- [x] Première génération Qwen réelle confirmée dans l’application ; tests de
  protocole et d’interface sur serveur simulé. [Guide IA](docs/AI_IMAGE_EDITING.md).
- [ ] Reprendre les essais Qwen prolongés et sur grandes images après rétablissement
  du serveur ; identifier la cause de son indisponibilité dans ses logs.

## Livraison de v0.21.0

- [x] Onglets et documents indépendants : calques, masques, sélection, tracés,
  historique, zoom, défilement et rotation de vue ; raccourcis configurables.
- [x] Ouvertures et créations sans remplacement, dépôt de plusieurs fichiers,
  activation d’un fichier déjà ouvert et développement RAW par document.
- [x] Fermeture d’un onglet et protection de tous les documents modifiés à Quitter.
- [x] Résultat IA dans un nouvel onglet à sa résolution native, masque redimensionné,
  source intacte ; contrôleur IA par document et résultats sans vol de focus.
  [Guide](docs/DOCUMENTS.md).
- [x] Clés enregistrées réutilisables entre profils non ambigus du même service,
  priorité au trousseau du profil après redémarrage, conservation sur adresse
  équivalente, fusion des indicateurs et statut explicite dans les réglages.
- [x] Plume sans outils d’ancrage redondants, curseurs distincts et migration
  des anciennes configurations. Conversion en forme dans la même entrée,
  couleurs et remplissage liés aux points ; remplissage raster clairement séparé.

## Livraison de v0.20.0

- [x] Importer des images comme calques via le menu et par dépôt dans la pile,
  en 8/16 bits et RAW, sans bloquer l’interface ; lot atomique annulable.
- [x] Ouvrir les images déposées dans la zone principale ou l’accueil et les
  fichiers transmis par le système, en protégeant les modifications non enregistrées.
- [x] Profils IA sans prompt, presets Topaz et Real-ESRGAN, source IA originale,
  paramètres de modèle/facteur et conservation des masques, placement et historique.
  Migration des anciens profils et détails d’erreurs HTTP sans données privées.
- [x] Panning par Espace avant le premier clic, curseurs main et isolation de la
  saisie des champs et dialogues. Tests moteur/interface et transport des couleurs.
- [ ] Comparer la qualité réelle de Real-ESRGAN sur les images de l’utilisateur ;
  aucun appel payant réalisé par les tests d’intégration.
- [x] Résultat d’upscale dans un document distinct : livré dans v0.21.0.

## Livraison de v0.19.0

- [x] Retirer le bandeau de marque et le nom d’outil ; garder les commandes
  communes dans la barre du document et compacter les réglages.

- [x] Trousseau IA lu uniquement à la demande de génération pour le fournisseur
  choisi, sans lectures au démarrage ni en ouvrant les réglages ; regroupement
  des accès et cache de session, sans relance automatique après refus.
- [x] Conteneur `.pcr` binaire v14 : tuiles Zstandard sans perte en 8/16 bits,
  fonds implicites, déduplication de plans/blocs, index CBOR, aperçu Quick Look
  indépendant et écriture atomique. Limite de fichier 8 Gio ; anciens JSON
  v1–v13 lisibles, avec migration au prochain enregistrement.
  [Spécification et limites](docs/PROJECT_FORMAT.md).

- [x] Aperçus Quick Look et miniatures Finder `.pcr` via deux extensions macOS
  natives, intégrées au paquet et sans Qt. Aperçu JPEG borné au début des
  nouveaux enregistrements, repli sur une image source des anciens projets,
  lecture réutilisée par les fichiers récents et ouverture Finder prise en charge.
  [Guide](docs/MACOS_QUICK_LOOK.md).

## Livraison de v0.18.2

- [x] Fournisseurs et prompts IA persistants, trousseau natif et 1–8 variantes.
- [x] Suivi visible, insertion unique et détails fournisseur/modèle/variante par calque.
- [x] Sources IA compactes dans les projets v13, avec repli natif après retouche.
- [x] Essais réels OpenAI et fal.ai confirmés par l’utilisateur.
- [ ] Essai ComfyUI avec serveur et workflow réels (protocole simulé déjà testé).
- [x] Synchroniser les contrôles de survol du dégradé, dessin des poignées de la
  Plume et premier affichage de l’aperçu du pinceau, sans retirer les assertions.
- [x] Attendre le rendu complet avant les comparaisons de bruit, mosaïque et
  filtres spatiaux, sans élargir leurs tolérances de pixels.
- [x] Publication et validation des paquets v0.18.2 sur les trois plateformes.
  [Workflow](https://github.com/fguilleme/PhotoCrap/actions/runs/42430277582).

## Correctifs de publication de v0.17.2

- [x] Stabiliser les conversions alpha 16 bits lors du mélange. Vérifier les
  comparaisons entre composition régionale et intégrale.
- [x] Synchroniser les assertions de survol du lasso polygonal et du recadrage
  avec les événements et les images produites par QML.
- [x] Vérifier les quatre empreintes raster variables à partir des deux jeux de
  références x86-64 observés en CI.
- [x] Valider les builds, tests et paquets du tag v0.17.2 sur les trois plateformes.
  [Workflow du tag](https://github.com/fguilleme/PhotoCrap/actions/runs/42133723208) ·
  [Release v0.17.2](https://github.com/fguilleme/PhotoCrap/releases/tag/v0.17.2)

## Optimisations du 4 novembre 2026 — non publiées

- [x] Courbes, Niveaux, Luminosité/Contraste, Exposition, Balance, Isohélie et
  Négatif composés dans la pile GPU par tables natives 8/16 bits, cache LRU borné
  et références transparentes logiciel/Metal. Suite dans [le plan](docs/OPTIMIZATION_PLAN.md).

- [x] Réglages tonals analytiques intégrés aux piles GPU natives, paramètres seuls
  actualisés pour les calculs directs ; 36 piles transparentes validées logiciel/Metal.

- [x] Contours progressifs des masques dans les piles GPU natives, passes et halos
  bornés et comparaisons aux bords et raccords avec le moteur CPU.

- [x] Lecture `.pcr` indexée avec buffer de 16 Kio, réservation logique avant
  décodage et validation d’une enveloppe de calque à la fois ; publication atomique.

- [x] Grands PNG natifs 8/16 bits et BMP composés/écrits par bandes, sans image
  d’export complète pour les piles locales ; annulation et fichier atomique.

## Pinceau mélangeur — non publié

- [x] Barre inspirée des commandes Adobe : charge multicolore, recharge/nettoyage
  automatiques visibles, douze combinaisons, pourcentages compacts, angle et aérographe.
- [x] Reprise de la peinture fraîche pendant le trait, échantillonnage multicalque
  régional, débit indépendant des combinaisons et accumulation sans mouvement.
- [ ] Pixels couleur 16 bits, pointes à soies et simulation de stylet (NYI).
- Les coefficients restent propres à PhotoCrap ; voir les [références et limites](docs/MIXER_BRUSH.md).

## Retouche native 16 bits — non publiée

- [x] Correcteur localisé, Suppression, Correcteur, Pièce, Déplacement basé sur le
  contenu et Œil rouge disponibles sur les pixels 16 bits, sans conversion en 8 bits.
- [x] Sources natives et composites, diffusion, textures, alpha et mélange final
  conservés ; masques de couverture toujours en 8 bits et mêmes limites régionales.
- [x] Sélections progressives, retouche sur calque vide, Annuler/Rétablir,
  sauvegarde/rechargement, calculs périmés et menu d’outils contrôlés.

## Optimisations du 3 novembre 2026 — non publiées

- [x] Règles de couverture partagées CPU/GPU, pile GPU limitée aux couches
  visibles ; effets incompatibles cachés ignorés, masques neutres sans passe et
  alphas natifs faibles conservés. Retour CPU dès que l’occlusion cesse.

- [x] Composite CPU ignorant les couches situées sous un aplat uniforme
  pleinement opaque ; effets, marges et lectures régionales limités à la pile visible.

- [x] Composite CPU ignorant les fonds natifs uniformes transparents et masques
  noirs uniformes opaques ; masques blancs ou de densité nulle reconnus avant
  expansion, flou et marges, en conservant les chaînes d’écrêtage.

- [x] Application des masques natifs 16 bits par multiplication entière exacte
  à densité maximale ; facteurs précalculés sans quantification aux autres densités.

- [x] Préparation des grandes sources de fréquences sur worker annulable,
  composite natif terminé réutilisé, sources préparées selon le budget RAM/disque ;
  ouverture des propriétés après publication, zoom et historique conservés.

- [x] Sources de fréquences directement partagées avec un masque blanc uniforme
  ou de densité nulle ; masques non neutres toujours appliqués aux pixels natifs.

- [x] Réutilisation des petits payloads de plans répétés pendant une sauvegarde,
  cache LRU de 8 Mio ; gros payloads en flux, sans expansion/réencodage sur hit.

- [x] Images et masques PNG identiques décodés une fois par format natif,
  copies indépendantes à l’édition et réservations logiques conservées.

- [x] Sources PNG identiques des fréquences décodées une fois par projet, plans
  et leases partagés ; reconstruction transparente préservée après réouverture.

- [x] Nouveaux documents blancs/transparents créés directement comme fonds natifs
  logiques ; aucune image pleine taille provisoire avant la première retouche.

- [x] Fonds compacts des fréquences étendus en plans de taille logique, sans
  pixels de canevas alloués ; versions distinctes et rendu natif inchangé.

- [x] Préparation des retouches de fréquences capturant les plans natifs partagés ;
  matérialisation des sources déplacée de la GUI au worker, leases disque préservés.

- [x] Baguette magique immédiate sur les sources uniformes reconnues : pas de
  lecture de pixels ni de tâche en arrière-plan ; effets spatiaux exclus.

- [x] Baguette magique en arrière-plan sur grands documents, annulation et
  invalidation des demandes périmées ; sélection et historique appliqués une fois.
- [x] Baguette magique lisant les tuiles natives à la demande, cache de comparaison
  borné et bits de visite alloués uniquement sur les tuiles explorées. Tolérance,
  connexité, combinaisons et historique comparés à une recherche indépendante.

- [x] Plans, masques et historique par tuiles partagées ; fonds compacts à la
  réouverture et conversions natives de profondeur sans canevas complet.
- [x] Emprises régionales des outils, pipettes, formes, texte, déplacements et imports ;
  sources composées des pinceaux, tampons et correcteurs figées par geste.
  Sélection rapide, contours progressifs des masques et copie de sélection régionaux,
  avec anciennes versions et masques conservés par Annuler/Rétablir.
- [x] Références de pixels spécifiques aux filtres Accelerate/portables, comparées
  au rendu intégral ; option CMake pour contrôler le chemin portable sur Mac.
- [x] Séparation de fréquences partageant la source native non masquée, sans copie
  complète supplémentaire ; retouches ultérieures isolées par versions de tuiles.
- [x] Ouverture d’images/projets/RAW et préparation des imports sur worker,
  avec contrôle de session/état, annulation et absence de capture inutile de
  l’ancien document lors d’une ouverture de remplacement.
- [x] Sauvegarde/export asynchrones, flux atomique PNG/Base64/JSON et fermeture
  différée après confirmation ; résultat périmé ou annulé rejeté.
- [x] Budget RAM des plans préparés, mappings transmis après chargement,
  repli RAM sur cache indisponible/saturé, déchargement annulable entre tuiles
  et collecte de l’historique hors interface.
- [x] Pile GPU et cinq filtres spatiaux avec halos, invalidation régionale,
  quatre niveaux de réduction et budget graphique partagé.

Les périmètres et mesures figurent dans le [plan des cinq chantiers](docs/OPTIMIZATION_PLAN.md),
le [guide mémoire](docs/LAYER_MEMORY.md) et la [validation](docs/VALIDATION.md).
Les essais physiques Vulkan/Direct3D, la perte réelle de périphérique et les
mesures de VRAM du pilote restent à réaliser sur les machines adaptées.

## Repères actuels vérifiés

| Élément | État du code | Source |
|---|---|---|
| Barre d’outils | 71 outils recensés : **56 disponibles, 15 NYI** | Catalogue de `qml/Main.qml` |
| Raccourcis et gestes | **303 entrées** : 135 commandes personnalisables, 58 gestes contextuels, 110 NYI | `src/ShortcutManager.cpp`, `src/ShortcutCatalog.h` |
| Projets | Format `.pcr` jusqu’à **v13** (sources IA compactes), texte éditable et lecture des versions antérieures ; sélections et historique temporaires | `src/DocumentIO.cpp`, [guide Sélections](docs/SELECTIONS.md) |
| Précision | Pixels RVB 8/16 bits, masques et couvertures de sélection 8 bits | [Précision et limites](README.md#raw-et-précision-des-couleurs) |

En développement après v0.16 : accueil redimensionnable avec grille de fichiers
récents, miniatures chargées en arrière-plan et aperçu JPEG intégré aux RAW.
Les projets `.pcr` sont composités en arrière-plan et leurs miniatures mises en cache.

Les sections « Disponible » et « Ajouts » ci-dessous constituent l’historique des fonctions livrées : leurs anciens nombres de raccourcis et formats de projet ne décrivent pas la version courante. Les sections numérotées regroupent les travaux restants et les validations à poursuivre. **NYI** signifie recensé mais non implémenté ; les gestes contextuels disponibles ne sont pas des commandes globales personnalisables.

## Disponible dans v0.12.0

- [x] Documents raster RVB sRGB 8 ou 16 bits par canal, ouverture/import RAW asynchrone via LibRaw et import d’images. Projets `.pcr` version 9 compatibles en lecture avec les versions 1–8 ; export aplati, avec précision 16 bits conservée en PNG.
- [x] Calques de pixels, Couleur unie, réglages non destructifs, groupes, écrêtage et 27 modes de fusion avec aperçu au survol.
- [x] Import d’images depuis le menu **+** des calques et par dépôt de fichiers dans la pile : position indiquée par une ligne, défilement aux bords, plusieurs images en une transaction asynchrone annulable, RAW et précision 8/16 bits.
- [x] Dépôt d’une image sur le canevas ou l’accueil pour l’ouvrir comme document, avec protection des modifications non enregistrées ; types d’images déclarés pour « Ouvrir avec » sur macOS et argument de fichier transmis par le lanceur Linux.
- [x] Masques raster : création, peinture, inversion, activation/désactivation, affichage isolé par Alt-clic et copie vers un autre calque par glisser. Alt-clic sur l’œil isole un calque puis restaure les visibilités.
- [x] Panneaux Histogramme, Navigateur, Couleur, Calques, Couches, Historique et Informations : redimensionnement, repli, déplacement, regroupement en onglets, colonnes, flottants et organisation sauvegardée. Visibilité contrôlée depuis Affichage → Panneaux.
- [x] Panneau Informations automatique pour les pipettes et la règle : lectures RVB, coordonnées, quatre échantillons, mesures et dimensions. Placement flottant ou ancré conservé.
- [x] Barre d’outils configurable : ordre des groupes et des outils, visibilité, variantes par clic droit, icône du dernier outil choisi et entrées futures désactivées avec mention NYI.
- [x] Réglages spécialisés : Lumière, Couleur, Niveaux, Courbes et pipettes, Balance des couleurs, Noir et blanc avec presets/Auto/Teinte, Filtre photo, Mélangeur de couches, Style colorimétrique (LUT), Transfert de dégradé, Correction sélective, Isohélie et Seuil.
- [x] Prévisualisation des réglages sur proxies, caches d’images et de masques, remplacement des demandes intermédiaires par la dernière, puis rendu complet en arrière-plan qui actualise le cache écran. Séparation de fréquences dynamique, puis préparation asynchrone de calques de pixels retouchables en 8/16 bits, avec annulation, masques conservés et retour au rayon depuis les sources enregistrées dans le projet.
- [x] Stockage des sorties IA à leur résolution d’origine dans les projets v13 : reconstruction locale 8/16 bits, masque indépendant, invalidation à la retouche des pixels et retour au raster natif sans perte des modifications. [Mesure de taille](docs/VALIDATION.md#stockage-compact-des-calques-ia--7-novembre-2026).
- [x] Création de calques IA : composition ou sélection, profils GPT Image/fal.ai/ComfyUI/API d’images compatible OpenAI, bibliothèque de prompts et 1–8 variantes par lancement ; clés persistantes dans le trousseau natif, liste de fournisseurs et sauvegarde explicite ; suivi asynchrone visible, masque original, insertion unique 8/16 bits annulable et résultat conservé si le stockage est saturé. Protocoles validés par serveurs simulés ; essais réels OpenAI/fal.ai confirmés par l’utilisateur, ComfyUI restant à tester. [Guide](docs/AI_IMAGE_EDITING.md).
- [x] Traitements IA sans prompt pour fal.ai/ComfyUI ; presets Topaz Upscale Precision et Real-ESRGAN x2plus, modèle et facteur ×1–×4, source IA d’origine du calque, envoi natif et limites vérifiées avant soumission. Résultat dans un nouveau document natif ou sur un calque aux dimensions de la source ; dialogue et protocole validés par simulations, qualité des upscalers à comparer sur les images de l’utilisateur.
- [x] Résultat IA dans un nouveau document à sa résolution native, avec onglets indépendants.
- [x] Fusions de calques : vers le bas, sélection, groupe, écrêtage, visibles, copies fusionnées des sélectionnés/visibles et aplatissement opaque. Masques/réglages rasterisés, sources cachées conservées pour Fusionner visibles, précision native 8/16 bits, worker annulable, sortie tuilée et raccourcis configurables. [Comportements et différences Adobe](docs/LAYER_MERGE.md).
- [x] Presse-papiers de sélection : Couper/Copier actifs, Copier avec fusion, nouveau calque au collage, sur place, dans/hors sélection avec masque éditable, calque par copie/couper et masque raster actif. Formes réelles, contours progressifs et pixels natifs 8/16 bits, calculs différés et historique ; [cas couverts et différences Adobe](docs/CLIPBOARD.md).
- [ ] Presse-papiers complet de calques et tracés : préserver texte éditable, formes, groupes, effets et masques associés entre documents ; gestion des profils couleur multiples.
- [x] Sélections rectangulaire, elliptique, rangée/colonne unique, lasso, lasso polygonal, sélection rapide et baguette magique ; combinaisons de sélection et contours animés.
- [x] Peinture : pinceau, crayon, gomme, remplacement de couleur, pinceau mélangeur, tampon de duplication et tampon de motif. Taille, dureté, opacité, débit, espacement, forme de pointe et lissage.
- [x] Retouche locale : flou, netteté, doigt, densité −/+, éponge, œil rouge, correcteur, correcteur localisé, pièce, déplacement basé sur le contenu et suppression. Calculs limités aux zones touchées et à leur voisinage lorsque le traitement le permet.
- [x] Forme d’historique et forme d’historique artistique avec source retenue, contrôles spécialisés et annulation par geste.
- [x] Pipette ponctuelle/moyenne, anneau comparatif, échantillonneur déplaçable et règle. Rectangle, ellipse, triangle, polygone, étoile et ligne raster avec options de remplissage/contour ; texte horizontal éditable en calque dédié (ponctuel/paragraphe, formats mixtes, polices, paragraphes et sauvegarde v9).
- [x] Création automatique de calques par Texte et les six formes raster ; choix Nouveau calque/Pixels sur calque actif dans leur barre. Pinceau/Crayon créant un calque sur cible non peignable selon une préférence persistante, masques conservés, insertion dans les groupes, annulation atomique du dessin et du calque, abandon du calque provisoire et limites mémoire/calques respectées. Les formes vectorielles restent à faire ; règles et écarts Adobe documentés dans [docs/TOOL_LAYERS.md](docs/TOOL_LAYERS.md).
- [x] Zoom centré sous le pointeur, déplacement temporaire par Espace, rotation de l’affichage et zoom conservé par Annuler/Rétablir.
- [x] Dialogues annulables pour le pinceau et les propriétés des calques, réinitialisation, boutons communs avec focus visible et comportement Retour/Espace. Retour valide d’abord l’édition d’un champ numérique.
- [x] Libellés numériques ajustables par clic-glisser horizontal : main au survol, Maj pour accélérer, Option/Alt pour affiner, bornes respectées et Échap pour annuler. Barres d’outils, dialogues et panneaux Lumière/Couleur ; validation unique des réglages et retour au texte après la typographie.
- [x] Interface française/anglaise, raccourcis personnalisables, fichiers récents, historique à budget mémoire et sauvegarde atomique.
- [x] Builds et paquets autonomes sur trois plateformes, checksums SHA-256 et publication GitHub depuis les tags `v*`. Contrôle de déploiement et de signature ad hoc du DMG ; tests moteur, QML et natifs macOS.
- [x] Recadrage avec cadre indépendant, ratios, dimensions de sortie, poignées et grille ; recadrage en perspective asynchrone des calques, masques et sources.
- [x] Tranche et Sélection de tranche : création, déplacement, redimensionnement, division, sauvegarde et export PNG des tranches utilisateur ; repères automatiques facultatifs.
- [x] Catalogue de 275 raccourcis et gestes référencés à Adobe, avec recherche et filtres disponibles/NYI/contextuels dans Réglages. Les fonctions manquantes sont recensées dans [docs/SHORTCUTS.md](docs/SHORTCUTS.md).
- [x] Interactions communes inspirées des références Adobe : Maj/Option pour les variantes, raccourcis temporaires, contraintes de tracé, combinaisons de sélections, Maj-clic de peinture, chiffres pour opacité/débit, réglage de brosse à la souris et curseur précis. [Guide détaillé](docs/TOOL_INTERACTIONS.md).
- [x] Jusqu’à 512 calques, budget de 2 Gio de données de calques, stockage compact des calques vides et masques uniformes, historique comptant les pixels supplémentaires, cache de fréquences borné à 128 Mio et liste de calques virtualisée.

## Ajouts de v0.13.1

Ces ajouts sont inclus dans le tag v0.13.1. La publication des paquets est automatique après validation des trois plateformes : [état du workflow](https://github.com/fguilleme/PhotoCrap/actions/workflows/build.yml). Le catalogue de v0.13.1 contient 282 entrées de raccourcis et gestes, dont 113 NYI après l’implémentation de la Gomme magique et de la Plume.

- [x] Plume en mode Tracé : Bézier, poignées, ajout/suppression/conversion de points, contraintes, reprise, fermeture, annulation et sauvegarde des tracés en projet v8. Conversion en sélection, remplissage et contour raster avec sélection et profondeur 8/16 bits conservées.
- [x] Conversion Plume → sélection sans points ni poignées superposés ; tracé conservé et réactivable, avec curseur Commande/Ctrl corrigé et test natif macOS.
- [x] Fermeture de l’application avec panneaux flottants/masqués et protection des documents non sauvegardés.
- [x] Gomme magique : effacement par couleur avec tolérance, contiguïté, lissage, opacité et échantillonnage visible de tous les calques. Calcul asynchrone annulable, sélection respectée, pixels 8/16 bits et une étape d’annulation par clic ; outil réservé aux calques de pixels.
- [x] Menus contextuels distincts pour calques et vignettes de masque : propriétés et verrouillage côté calque, affichage/activation/inversion/suppression côté masque. Les actions portant sur un seul calque sont désactivées pour une sélection multiple ; les fonctions manquantes sont marquées NYI. Tests de clic droit ciblant un autre masque, d’annulation et de verrouillage.

## Ajouts de v0.14 : sélections et dialogues

- [x] Menu Sélection structuré selon la référence Adobe, au-delà des entrées visibles dans la capture. [Inventaire des comportements, options et écarts](docs/SELECTIONS.md).
- [x] Frange (1–200 px), Lisser, Dilater et Contracter (1–100 px), calcul asynchrone régional annulable, rejet des résultats obsolètes et historique des sélections sans modifier les pixels sauvegardés.
- [x] Contour de sélection masquable depuis Affichage, réaffiché à la création suivante ; Calques similaires par nature du calque ; raccourcis configurables. Catalogue de v0.14 : **298 entrées, dont 121 NYI**.
- [x] Comptabilisation des régions et contours partagés dans le budget mémoire de l’historique.
- [x] Couverture progressive 0–255, Contour progressif (rayon 1–1 000 px, Maj+F6), conservation dans l’historique et Resélectionner ; prise en compte par les opérations de pixels et les masques.
- [x] Plage de couleurs : pipettes remplacer/ajouter/retirer sur l’aperçu et le canevas, tolérance, groupes localisés, zones contiguës, inversion et aperçus image/masque synchronisés avec zoom natif et déplacement ; familles de couleurs et de tons.
- [x] Dialogues de fichiers Qt Quick lisibles en thème sombre : palette complète, repli adapté à Qt 6.8 et test `file_dialog_fallback` pour ouvrir, enregistrer, exporter et choisir un dossier. Les dialogues natifs restent utilisés lorsqu’ils sont disponibles.

## Ajouts de v0.15 : calques, mémoire et cache

- [x] Éviter les copies pleine résolution des sources et du composite à chaque calque en mode Normal, ainsi que la copie finale du résultat du worker.
- [x] Écarter les plans invisibles de la préparation des proxies, sans casser les chaînes d’écrêtage ni la précision du rendu complet.
- [x] Réutiliser une bande haute encore dans le LRU sans recalculer sa bande basse évincée ; conserver le plafond de 128 Mio.
- [x] Ajouter une pile de 64 calques remplis en 8/16 bits, comparée à une référence de rendu, avec retouche locale et Annuler/Rétablir ; tester l’éviction des fréquences et les calques/groupes masqués.
- [x] Première étape du cache disque : blocs natifs 8/16 bits vérifiés par SHA-256, mappings en lecture seule, budgets, éviction, nettoyage et protection des sessions actives. Branché sur les bandes de fréquences d’au moins 16 Mio, dans les workers ; configuration et vidage depuis Réglages → Cache de travail.
- [x] Moteur de plans parcimonieux par tuiles de 256 × 256, versions partagées et stockage disque avec références indépendantes de l’index du cache. Premier usage : couverture du pinceau, du crayon et de la gomme, avec tests 8/16 bits, masques, débit et annulation. Les plans de calques, masques et sources sont désormais raccordés aux tuiles ; certains outils utilisent encore un tampon temporaire complet.

## 1. Fiabilité et validation à terminer

- [x] Curseurs natifs macOS corrigés à la première activation des pipettes et de la règle, après réinitialisation par AppKit et après ouverture d’Informations ; gestes Commande de la Plume couverts par les tests natifs. Ces contrôles passent dans le workflow v0.14 sur Qt 6.8.3.
- [ ] Étendre ces contrôles de curseurs aux autres outils, transitions de focus, écrans multiples et fenêtres flottantes, au-delà des scénarios automatisés actuels.
- [ ] Étendre les tests d’espaces de travail : déplacements entre colonnes, sorties de groupes, dépôt avant le premier panneau, écrans multiples, changement de résolution et restauration de fenêtres hors écran.
- [ ] Étendre les essais au clavier, l’accessibilité, les curseurs, la haute densité et les tablettes sur les trois plateformes.
- [x] Version CMake et noms des paquets alignés sur 0.15.0 (tag v0.15) ; l’AppImage reprend automatiquement la version configurée.
- [ ] Signature Developer ID et notarisation macOS ; signature Authenticode Windows. Les signatures de distribution ne sont pas encore configurées.

## 2. Grands documents et performance

Le programme des cinq migrations de performance demandées le 2 novembre est
détaillé dans [docs/OPTIMIZATION_PLAN.md](docs/OPTIMIZATION_PLAN.md). Le raccordement
des plans de calques par tuiles, la composition GPU, les filtres spatiaux,
l’asynchrone commun et les niveaux de détail GPU ont maintenant une base validée.
Les extensions de novembre couvrent aussi les déplacements denses, les dégradés pleine image, les petits deltas d’historique et les photos opaques couvrantes. La campagne finale passe en logiciel, Metal et OpenGL Core ; documents et stockage sont aussi contrôlés sur Qt 6.8.3 et sans Accelerate. Les validations physiques Vulkan/Direct3D restent à réaliser sur le matériel concerné.

Les proxies et le rendu asynchrone des réglages sont déjà disponibles. Il reste à couvrir les chemins encore synchrones et à réduire le coût mémoire des grandes images.

- [x] Jusqu’à 512 calques, stockage compact des calques vides et masques uniformes dans les projets v7 et suivants, allocation différée lors de la peinture, historique comptant les pixels supplémentaires et cache LRU de fréquences limité à 128 Mio. Test de stockage/rechargement de 301 calques sur une image 24 Mpx en 16 bits : les calques ajoutés sont vides et les réglages masqués ; ce n’est pas un test de 301 photos remplies. Liste de calques virtualisée.
- [ ] Étendre la composition GPU aux piles encore non prises en charge et poursuivre les traitements régionaux des outils hors empreintes.
- [x] Pile GPU native : pixels, texte, Couleur unie et 18 réglages tonals, masques progressifs, groupes et écrêtage. Tuiles de 512 px, RGBA8 ou RGBA32F et quantification native ; 25 fusions générales, Fondu et Mélange maximal conditionnels. Les effets non pris en charge et combinaisons non exactes gardent le CPU ; l’export reste natif CPU.
- [x] Trois paliers de ressources GPU : entrées conservées, entrées réutilisées entre passes, puis accumulateurs/filtres/réductions partagés entre tuiles. Sortie indépendante, ressources caduques libérées et budget vérifié après les zooms.
- [x] Ouverture `.pcr` indexée : métadonnées puis enveloppes par calque, préflight du stockage avant décodage et validation du JSON ; aucun agrégat de tous les PNG/Base64.
- [x] Grands exports PNG natifs 8/16 bits et BMP par bandes : flux compressé borné, annulation, publication atomique et références décodées exactes. Réutilisation du composite possédé par les codecs Qt JPEG/WebP, qui gardent un tampon complet.
- [x] Sondes opt-in de latence, RSS et compteurs graphiques, avec rapports JSON et validation du backend réellement exécuté. [Procédure et limites](docs/OPTIMIZATION_PROBES.md).
- [x] Cinq filtres spatiaux en passes GPU tuilées : boîte, gaussien, directionnel, netteté et passe-haut. Halos cumulés, alpha/masques, arrondis natifs et cibles réutilisées. Aperçu indépendant du worker CPU, mises à jour auxiliaires limitées à 100 ms et Annuler immédiat. Le rendu final reste CPU ; formats et budget insuffisants conservent leur repli.
- [x] Calcul asynchrone commun pour les transformations, profondeur depuis le menu, fusion vers le bas, remplissages/effacement, inversion des masques et filtres directs dès quatre millions de pixels traités. Progression, annulation immédiate et validation atomique en une étape ; fermeture/changement de document testés. Ouverture des projets et images, export et sauvegarde utilisent aussi le répartiteur en lecture seule.
- [x] Aperçus CPU rapides par lecture régionale des tuiles, sans reconstruire le plan natif avant réduction. Référence exacte Qt en 8/16 bits et masques, y compris ses arrondis par axe ; agrandissements et interpolation lisse gardent leur chemin complet.
- [x] Retouches Flou, Netteté, Densité −/+ et Éponge : snapshot, couverture et écriture par régions tuilées, halos de voisinage conservés et correction de la lecture de couverture 16 bits. Tests de frontières, sélection progressive, annulation et allocation locale sur 24 Mpx.
- [x] Œil rouge : lecture de la seule zone de recherche et écriture du rectangle corrigé par tuiles, sans matérialiser le calque. Résultats identiques pour dix-huit combinaisons clic/cadre/réglages, reflets et sélection conservés ; test mémoire 24 Mpx/8 bits.
- [x] Remplissage de tracé Plume régional sur pixels 8/16 bits et masques : précision native, sélection progressive et historique conservés. Référence pleine image avec écart d’anticrénelage explicitement borné, test mémoire 24 Mpx/16 bits.
- [x] Formes géométriques et dégradés sélectionnés : aperçu régional sur snapshot tuilé, marge des jointures conservée, invalidation des anciennes emprises et retour au point de départ sans calque vide. Références natives 8/16 bits, 36 combinaisons, test de petite forme sur 24 Mpx et historique. Extension du dégradé pleine image aux bandes natives bornées, coordonnées globales et références QPainter exactes 8/16 bits.
- [x] Déplacement d’une sélection par régions source/destination séparées, sans allouer leur intervalle. Références exactes de douze combinaisons 8/16 bits, copie Alt, transparence et sélection progressive ; test 24 Mpx, disparition des anciens aperçus et historique des sélections sorties du canevas. Déplacement du calque entier encore complet.
- [x] Déplacement de calques parcimonieux sur fond transparent par tuiles, sans tampon complet : copie Alt, aperçu, sortie du canevas, annulation et historique. Références QPainter exactes 8/16 bits, bord natif presque transparent après déchargement et test 24 Mpx ; Extension aux calques denses, fonds uniformes compacts et retour sans historique au point de départ ; références exactes, copie Alt et test de bord natif 1 × 1.
- [x] Rasterisation régionale du texte éditable sur plan tuilé : saisie, mise en forme, déplacement et redimensionnement sans tampon systématique du document entier. Références natives exactes de 24 cas 8/16 bits, paragraphes transformés/multilingues et clipping ; test mémoire 24 Mpx, retour à l’ancre sans historique et copie Alt annulable.
- [x] Export des grandes images en arrière-plan depuis le dialogue, sur snapshot immuable, sans changement d’historique/état enregistré. Références binaires des PNG/JPEG/BMP/WebP disponibles en 8/16 bits, erreurs et annulation ; test réel d’un PNG 16 Mpx/16 bits interrompu avant publication. Validation finale du fichier non annulable une fois engagée.
- [x] Sauvegarde des grands projets sur snapshot en arrière-plan : progression par plan, annulation avant publication et marquage « enregistré » après validation uniquement. Enregistrer puis fermer attend la réussite ; annulation/erreur conserve la fenêtre ouverte. Références binaires 8/16 bits, signaux stables et test réel d’annulation 16 Mpx/16 bits.
- [x] Sauvegarde `.pcr` v9 en flux atomique : PNG/Base64 et calques JSON écrits progressivement, sans agrégat complet encodé ; fragments Base64 de 16 Kio, limite réelle de 256 Mio et fichier précédent conservé en cas d’échec. Références exactes de payloads natifs, écritures courtes, erreurs, padding et métadonnées échappées ; petites sauvegardes immédiates, grands projets en arrière-plan.
- [x] Pot de peinture parcimonieux : lecture native de tuiles partagées, masque de visite alloué par tuiles et mélange limité aux emprises remplies. Contiguïté/alpha/sélection/masques conservés, référence exacte de seize cas, test d’une petite composante sur 24 Mpx après déchargement et invalidation régionale.
- [x] Pot de peinture asynchrone dès quatre millions de pixels dans l’emprise de recherche : progression, annulation pendant le parcours, application atomique et une étape d’historique. Comparaisons exactes 8/16 bits/masques, réglages capturés, fermeture/changement de document et priorité du geste suivant dès la pression.
- [x] Tampons Duplication/Motif : couverture et destination tuilées sur pixels et masques, sources natives ou composées figées au début du geste, lectures régionales aux décalages fractionnaires. Références QPainter en 8/16 bits et tests d’allocation sur 24 Mpx.
- [x] Doigt : destination tuilée, poche et pointe bornées au pinceau et prélèvements régionaux, y compris en mode « Tous les calques ». Les sources figées du Doigt, du tampon et des correcteurs partagent une fenêtre de prélèvement limitée à 256 Kio par geste.
- [x] Pinceau mélangeur : prélèvements Alt régionaux, prise de couleur locale pendant le geste humide et peinture à sec sans lire la source. Réservoir, masques et historique conservés.
- [x] Correcteur et Correcteur localisé : source du calque actif partagée par tuiles, diffusion et recherche de donneurs dans des régions bornées, destination limitée aux pixels modifiés. Références exactes pour les seize combinaisons, historique et allocations locales 24 Mpx ; couverture pleine image retirée du contrôleur commun des pinceaux.
- [x] Plans d’un pixel rééditables après stockage tuilé : expansion depuis le pixel courant et clés de rendu natives, conservation des versions identiques, métadonnées et Annuler/Rétablir en 8/16 bits et masques.
- [x] Signatures des bordures limitées aux pixels de l’intersection native : empreintes partielles réutilisées et bornées, clés rapides pour les tuiles entières. Conservation des textures GPU après une modification voisine sans changement de leurs pixels ; tests natifs 8/16 bits, masques, concurrence et sources mappées.
- [x] Écritures régionales sans copie pour les tuiles identiques : identifiants et mappings conservés, détachement limité aux tuiles réellement différentes et plans uniformes conservés compacts. Tests 8/16 bits/masques, quatre tuiles traversées, modification d’un seul pixel et vidage du cache.
- [x] Remplacement de couleur : source, couverture et destination par tuiles, mêmes résultats pour les 36 combinaisons de modes/échantillonnages/limites. Pixels extérieurs et historique inchangés, test d’allocation locale sur 24 Mpx/8 bits ; profondeur prise en charge inchangée.
- [x] Forme d’historique : source conservée par versions de tuiles et mélange/couverture/destination régionaux. Références transparentes, états futurs et branches supprimées conservés ; test 24 Mpx après déchargement et vidage du cache.
- [x] Forme d’historique artistique : source, couverture et destination tuilées, couleurs/gradients lus dans une petite région par empreinte. Pixels des dix styles et dispersion déterministe identiques, tolérance et historique conservés, tests d’allocation 24 Mpx et de groupes extrêmes aux bords.
- [x] Quatre niveaux de détail GPU conservés (1/2 à 1/16) pour les piles éligibles, grille globale et halos sans raccord, réutilisation des zooms et reconstruction après recréation du graphe. Repli CPU pour les piles, formats ou budgets incompatibles.
- [x] Budget graphique partagé entre canevas : photos, masques, effets, pile/intermédiaires et damier, ressources visibles épinglées et réserve hors écran LRU. Compteurs par catégorie et pic ; cible d’éviction, hors allocations internes Qt/pilote.
- [x] Sondes de latence, mémoire et précision pour les cinq filtres spatiaux en 8 et 16 bits, y compris le proxy logiciel. Sonde de latence pour Pièce et mesures indépendantes de l’écriture et de l’ouverture des grands projets. [Procédure et scénarios](docs/OPTIMIZATION_PROBES.md)
- [x] Opacité complète mémorisée par version de tuile, sans relire les mappings ; exclusion CPU/GPU des photos natives opaques couvrantes, groupes et masques contrôlés. Imports préparés nativement et RAM des plans courants séparée de celle de l’historique.
- [x] Deltas intra-tuile pour les petites retouches de l’historique : rectangles natifs, huit prédécesseurs maximum, fusion des versions jetables et comptabilité des blocs partagés. Références exactes 8/16 bits et masques, déchargement et cache vidé.
- [x] Stockage de travail par tuiles avec cache disque configurable pour pixels, masques, sources et historique ; déchargement asynchrone, contrôle d’espace libre et refus sans perte sur volume saturé. Les outils à empreintes lisent/écrivent des régions et les pics de RAM sont mesurés sur un scénario dense 24 Mpx/16 bits. Ce budget n’est pas un plafond global du processus. [Audit, implémentation et limites](docs/LAYER_MEMORY.md).
- [x] Encodage par bandes et décodage en tuiles des grands plans PNG compatibles dans les projets `.pcr`. JPEG par bandes et alimentation des plans YUVA de WebP depuis des bandes du composite.
- [x] Réparation Pièce limitée à la cible, au donneur et à leur voisinage. Assemblage du Déplacement basé sur le contenu dans les tuiles source et cible, avec une couverture progressive locale. Composition régionale de Fondu sans écrêtage et de Bruit sur une base opaque. Les cas qui diffèrent de la référence restent en rendu intégral.
- [x] Lecture Base64 des projets indexés par blocs de 16 Kio, avec validation des chaînes JSON et partage des PNG identiques. Sur le projet de référence à quatre calques de 24 Mpx en 16 bits, le pic RSS médian à l’ouverture passe de 803 à 776 Mio.
- [x] Mosaïque régionale calée sur la grille du document. Les tests comparent le rendu intégral et régional en 8/16 bits, sur des dimensions impaires et des images d’une seule ligne ou colonne.
- [x] Composition régionale des bandes dynamiques de fréquences natives. Les basses fréquences et les hautes fréquences compatibles lisent les tuiles utiles avec un halo, même à froid. Les hautes fréquences 8 bits avec transparence conservent le rendu intégral pour garder les mêmes arrondis.
- [ ] Étendre les mesures de latence et de mémoire aux outils restants.
- [ ] Historique par différences, récupération après fermeture anormale et autosauvegarde, distincts du cache temporaire de travail.
- [ ] Documents multiples et espaces de travail nommés/importables.
- [x] Première étape d’affichage après v0.15 : canevas en textures du graphe de scène Qt Quick, blocs visibles réutilisables, niveau de détail d’affichage conservé et surimpressions séparées de la photo ; repli logiciel et test `canvas_render`. [Fonctionnement et limites](docs/GPU_RENDERING.md).
- [x] Premiers aperçus GPU Courbes/Niveaux : base écran opaque réutilisée et table RVB issue du moteur CPU, actualisation auxiliaire du Navigateur/histogrammes, repli CPU pour les cas non pris en charge et rendu final natif 8/16 bits. Tests de référence CPU, d’annulation et de textures sur Metal (M2 Pro) ; autres backends matériels à vérifier.
- [x] Aperçu GPU du panneau Lumière : Exposition, Contraste, Hautes lumières, Ombres, Blancs et Noirs via une table de luminance CPU et une reconstruction couleur dans le shader. Réutilisation de la base écran, historique et rendu final 8/16 bits ; contrôles extrêmes et couleurs saturées comparés à la référence sur Metal.
- [x] Aperçus GPU Couleur et Balance des couleurs : paramètres Température/Teinte/Vibrance/Saturation calculés directement dans le shader, table RVB partagée avec le calcul CPU de Balance ; base réutilisée, repli logiciel et rendu final 8/16 bits. Comparaison des couleurs et valeurs extrêmes sur Metal et OpenGL en boucle principale.
- [x] Aperçus GPU Noir et blanc et Transfert de dégradé : six familles calculées dans le shader, palettes de gris/colorisation et de dégradé issues du moteur CPU. Réutilisation des textures photo, annulation et rendu final natif 8/16 bits ; couleurs saturées/atténuées, extrêmes des familles et teintes comparés sur Metal et OpenGL en boucle principale.
- [x] Aperçus GPU Filtre photo et Mélangeur de couches via des matrices couleur : densité et préservation de luminosité, coefficients et constantes RVB/monochromes. Paramètres transmis sans recréer les textures ; repli CPU et rendu final natif 8/16 bits. Comparaisons des valeurs extrêmes sur Metal et OpenGL en boucle principale.
- [x] Aperçu automatique des menus de styles Noir et blanc, Filtre photo et Mélangeur de couches au survol et au clavier ; validation unique, restauration sur Échap/fermeture et respect du contrôle Aperçu. Composant commun et test d’interface `preset_preview`.
- [x] Aperçus GPU Isohélie et Seuil : table de paliers issue du CPU et luminance entière pour le seuil noir/blanc ; base écran réutilisée, annulation et rendu final natif 8/16 bits. Comparaison dense des rampes, des 31 réglages d’Isohélie et des limites de Seuil sur Metal et OpenGL en boucle principale.
- [x] Aperçus GPU Style colorimétrique et Correction sélective : six styles, intensité/luminosité, neuf familles et modes relatif/absolu. Paramètres de shader regroupés dans un bloc de 224 octets, textures réutilisées et rendu final natif 8/16 bits. Comparaisons Metal/OpenGL en boucle principale et repli CPU testé pour les anciens projets à correction globale.
- [x] Aperçus GPU des anciens réglages Luminosité/Contraste, Exposition et Vibrance : tables CPU ou calcul direct conservant leurs règles propres, dont le facteur négatif de Vibrance. Calculs CPU Exposition/Vibrance séparés, rendu final 8/16 bits et alpha conservés. Registre commun au moteur et aux tests de rendu pour couvrir les seize réglages.
- [x] Aperçus GPU Teinte/Saturation (conversion HSL) et table fixe de Négatif ; calculs CPU séparés et Négatif sans conversions QColor par pixel. Correction des gris devenant noirs avec une teinte négative. Couverture des dix-huit réglages, de 135 combinaisons HSL et de la précision/alpha 16 bits ; ajout de Négatif et rendu final toujours sur CPU asynchrone.
- [x] Composition des aperçus GPU en mode Normal avec opacité réduite et masques uniformes blanc/gris/noir, densité et contour progressif. Paramètres sans texture supplémentaire, tables identiques réutilisées, historique/annulation et rendu final natif 8/16 bits préservés. Comparaison aux compositions QPainter sur les dix-huit réglages ; masques peints et autres modes restent sur CPU.
- [x] Aperçus GPU des réglages avec masque peint : proxy de masque préparé en arrière-plan et blocs visibles alignés sur la photo. Densité sans retransfert, contour progressif ne remplaçant que le masque, conservation de la base écran, rejet des demandes périmées et rendu final natif CPU. Tests des dix-huit réglages, des frontières de blocs, du zoom, de l’annulation et du redimensionnement ; peinture toujours sur CPU.
- [x] Quatorze modes de fusion pour les aperçus GPU des réglages, dont Produit, Écran, Incrustation, Lumières crue/linéaire/ponctuelle, Différence et Exclusion. Masque prémultiplié puis mélange selon la référence CPU ; survol sans retransfert de textures ni agrandissement du bloc de paramètres. Tests des dix-huit réglages, historique/annulation en 8/16 bits et repli des autres modes. Composition des calques de pixels toujours sur CPU.
- [x] Fusions GPU Teinte, Saturation, Couleur et Luminosité pour dix réglages à entrée exacte, sans atténuation du masque. Conversion HSL partagée et quantifiée comme QColor, opacité du calque conservée et repli CPU explicite pour les autres combinaisons. Comparaisons de pixels, historique/annulation et rendu final 8/16 bits ; changement de fusion sans retransfert de photo/table.
- [x] Aperçus GPU Densité couleur +/− et Division pour les dix réglages à entrée exacte et couverture de masque complète. Rapports calculés depuis les canaux entiers, branches zéro/255 conservées et aucun retransfert de photo au changement de mode. Tests des limites, opacités 37/100 %, historique/annulation en 8/16 bits et replis CPU partagés avec les fusions HSL.
- [x] Aperçus GPU Couleur plus foncée/claire et Lumière tamisée/vive pour les dix réglages à entrée exacte et couverture de masque complète. Comparaisons de sommes RVB entières, égalités conservant la base et formules de contraste inchangées. Tests des quarts/demi-tons, sources proches de zéro, valeurs connues, historique et rendu final 8/16 bits. 25 modes éligibles selon les cas à cette étape.
- [x] Aperçus GPU Fondu et Mélange maximal sans interpolation : motif de Fondu stable dans les coordonnées du proxy, seuil d’opacité entier et origine des tuiles ; décisions de Mélange maximal aux frontières partagées avec le moteur CPU dans une seconde ligne de table. Couverture de masque complète requise et dix réglages à entrée exacte pour Mélange maximal. Les 27 modes sont désormais éligibles selon les conditions ; rendu final natif toujours CPU. Tests des 256 frontières complémentaires, opacités limites, changement de zoom, tuiles et historique 8/16 bits.
- [x] Transferts régionaux des textures au niveau de détail entier : le composite identifie sa zone de sortie avec ses clés avant/après, et seuls les blocs touchés et leurs raccords sont remplacés. Prise en compte des halos de filtres et masques adoucis ; repli complet pour les coordonnées absolues ou les versions intermédiaires manquantes. Tests de pixels et de transferts 8/16 bits, frontières, hors champ, déplacement, annulation et niveaux réduits.
- [x] Transferts régionaux aux niveaux d’affichage réduits : mise à jour partielle du cache sur les grilles de réduction entières, sans conserver le composite natif. Dimensions irrégulières redimensionnées intégralement par Qt pour conserver leur phase d’échantillonnage, mais seules les tuiles touchées sont retransférées. Comparaisons exactes à Qt avec alpha et 8/16 bits, tests de raccords/hors champ aux niveaux 1/2 et mesure séparée des pixels rééchantillonnés.
- [x] Réutilisation des tuiles de masque inchangées pendant les aperçus GPU : comparaison des pixels visibles et de leurs bordures sans recopier les blocs identiques, clés de texture indépendantes par tuile et suppression correcte des textures lors de la désactivation du masque. Densité sans transfert ; contour progressif ne retransférant que les blocs modifiés. Tests de références couleur en 8/16 bits, raccords, niveaux réduits et formats Grayscale8/RVB/RGBA64. Le calcul du proxy et de son flou reste intégral sur CPU en arrière-plan.
- [x] Cache graphique de navigation LRU réglable dans Préférences → Cache de travail (64 Mio par défaut, 0–1 024 Mio) : conservation des tuiles photo/masque récemment affichées, éviction des moins récentes et retrait des nœuds hors champ du dessin. Budget estimé des textures du canevas ; zone visible prioritaire même si elle le dépasse. Invalidation des retouches hors champ et suivi différé des versions de masque, sans conserver le composite natif. Tests de budget, persistance, liaison QML, navigation 8/16 bits et masques modifiés hors champ.
- [x] Contours animés des sélections générales sur GPU : géométrie de trait limitée à la vue, pointillés anticrénelés animés par une constante de shader, sans recréer d’image ni de géométrie à chaque phase. Phase conservée au déplacement, transformations du graphe de scène et découpage de la vue. Repli QPainter sur backend logiciel ou au-delà d’un million de sommets ; rectangles/ellipses/lassos QML conservés. Tests de trous, découpage, grandes coordonnées, phases fractionnaires, rotation et absence de remplissage.
- [x] Marquage de l’outil Suppression en géométrie GPU : rectangles disjoints de la région limités à la vue, mélange translucide sans texture intermédiaire, trous et annulation préservés. Découpage local commun aux contours de sélection, y compris sous rotation. Repli QPainter logiciel ou au-delà d’un million de sommets. Tests d’aire, trous, coordonnées, transparence, animation simultanée et protection des pixels 16 bits ; la reconstruction Suppression reste CPU et prend désormais en charge les pixels 8/16 bits.
- [x] Aperçu de Pièce en texture GPU bornée, avec géométrie texturée suivant la région et ses trous. Aucun second rendu de la taille de la vue ; Source réutilise la géométrie et Destination conserve les textures donneuses identiques. Transparence, déplacement, découpage, rotation et annulation comparés à la référence raster. Repli logiciel, complexité et allocation ; la préparation du donneur et la réparation finale restent CPU.
- [x] Réparation de Pièce asynchrone au relâchement : adaptation locale, comparaison des pixels et copie du calque sur worker ; aperçu conservé jusqu’au composite réparé, une seule annulation, abandon par Échap/Annuler et rejet des résultats obsolètes. Tests Source/Destination, contour progressif, transparence, gestes rapprochés, changement de calque/document et absence de modification.
- [x] Assemblage final du Déplacement basé sur le contenu sur worker : copies locales, raccords Source/Destination, transparence, comparaison et copie du calque complet hors du thread GUI ; couverture douce limitée aux régions touchées sans parcourir leur espace vide. Aperçu conservé jusqu’au composite, annulation des calculs obsolètes et protection de la sauvegarde/fermeture. Tests des modes Déplacement/Extension, chevauchement, contour progressif, tous les calques et rendu Metal/OpenGL.
- [x] Finition de Suppression sur worker : développement différé des calques compacts, copies et assemblage des pixels, comparaison et couverture douce limitée aux marques. Installation du résultat sans copie complète sur le thread GUI, annulation sans résultat tardif et protection de la fermeture/sauvegarde pendant le calcul. Tests des calques vides, échantillonnage global, transparence, trous, contour progressif, absence de modification et rendu logiciel/Metal/OpenGL.
- [x] Contour progressif de la Gomme magique fusionné avec l’effacement sur worker en 8/16 bits : interpolation native uniquement sur les pixels correspondants, aucun second détachement complet sur le GUI et rectangle modifié calculé après atténuation. Annulation lors d’un changement de réglage, garde commune des quatre retouches asynchrones pour Annuler, fermeture et sauvegarde. Comparaisons au mélange séparé, lissage, contiguïté, opacité, tous les calques et transparence à l’écran Metal/OpenGL.
- [x] Contours de rectangle et d’ellipse dans le chemin commun GPU/logiciel du canevas : courbes elliptiques directes, trait cosmétique comme les sélections générales, géométrie mise en cache par forme/rectangle/zoom/vue et phase animée sans texture. Suppression du Canvas QML d’ellipse et du repeint QML pour le rectangle ; lignes/colonnes uniques et lassos conservés. Tests du changement rectangle→ellipse aux mêmes dimensions, courbure, absence de remplissage, animation fractionnaire, déplacement, rotation et masquage sur Metal/OpenGL.
- [x] Contours des lassos libre et polygonal terminés dans le chemin GPU/logiciel commun : points natifs, fermeture et concavités conservées, géométrie réutilisée pour la phase et invalidée au déplacement du contour. Guides en cours conservés en QML et limités à la vue ; disparition du tracé candidat QML après validation, y compris pour une sélection combinée. Tests des transitions, annulation, retouches Pièce/Déplacement, phases fractionnaires, clipping, rotation, 8/16 bits et pixels inchangés.
- [x] Contours de rangée et colonne uniques dans le chemin GPU/logiciel commun : bande d’un pixel natif suivant le zoom, géométrie limitée à la vue, phase sans retransfert et repli raster sous forme de bande étroite. Suppression du dernier Canvas QML de sélection rectangulaire. Tests 8/16 bits aux zooms 0,5×/1×/4×, clipping, rotation, pixels préservés et maillage borné pour 16 384 pixels à 8×.
- [x] Candidat du lasso libre sur le contour GPU commun dès qu’il forme une zone : points natifs, animation sans texture et réutilisation de la dernière géométrie au relâchement. Même chemin lors du détourage initial de Pièce et Déplacement basé sur le contenu ; repli logiciel borné. Guide ouvert initial, poignées polygonales et candidat combiné conservés en QML. Tests du lasso en 8/16 bits et des deux retouches en 8 bits : geste, mouvement, masquage, annulation, historique et absence de doublon QML.
- [x] Surimpressions de mesure bornées et séparées du panneau principal : anneau de pipette de 84 × 84 pixels logiques, déplacé par le graphe de scène sans repeindre à couleur constante ; règle limitée à l’emprise visible de son segment et de ses poignées. Tests de rendu sur une géométrie 16K à 16× : couleurs, absence de repeint au déplacement, coordonnées après défilement/rotation, règle traversant le document et disparition au masquage.
- [x] Contour du pinceau dans un composant borné à la vue : dessin local réutilisé au déplacement sans découpage, repeint limité lors du découpage, conservation de l’angle/aplatissement et des marges hors image visibles. Signes +/− de Sélection rapide positionnés dans les limites et gardés droits. Tests logiciel/Metal/OpenGL des pixels, absence de repeint, ellipse, clipping, rotation, repositionnement et empreinte de 32 000 pixels affichés ; liaison réelle du curseur à la souris dans le smoke test QML.
- [x] Surimpression Plume séparée en courbe, segment provisoire et poignées, dans leur ordre de dessin : surfaces limitées aux enveloppes visibles, snapshot des points conservé entre notifications et survol sans repeindre les parties fixes. Modifications/annulation réactualisant les caches, fermeture et désactivation supprimant le guide. Comparaisons à l’ancien dessin en 8/16 bits, zoom/défilement, compteurs de repeint et gestes réels du smoke test QML.
- [x] Guides de lasso extraits et bornés à leur enveloppe visible : snapshot natif conservé entre changements, vidé lorsque le contour passe au canevas partagé, et aucune animation de phase pour les guides unis. Dessin en une passe conservant l’anticrénelage ; candidats combinés et transition Alt gardant leurs pointillés. Comparaisons à l’ancien rendu en 8/16 bits, zoom/défilement, masquage, validation, annulation et vérification des repeints/du minuteur dans l’interface.
- [x] Cache natif des descriptions de tranches par dimensions, rectangles manuels et affichage automatique : buffer partagé réutilisé pendant le survol, la sélection et les gestes provisoires, invalidation naturelle après édition/Annuler/Rétablir/ouverture. Snapshots QML partagés dans les contrôles et le canevas ; rendu limité aux tranches visibles avec marge pour étiquettes/poignées, sans repeint sur les réglages sans rapport. Tests de partition fragmentée plafonnée à 1 024 descriptions, buffers, projets et rendu 8/16 bits comparé à l’ancien canevas.
- [x] Voile des recadrages classique et perspective sur GPU : rectangle de quatre sommets, ouverture par quatre plans et mise à jour des paramètres sans texture ni rechargement de géométrie au déplacement. Coordonnées locales indépendantes du regroupement Qt ; cadre/grille/poignées raster limités à leur emprise visible, snapshot des points réutilisé au survol et repli logiciel complet. Comparaisons à l’ancien rendu en 8/16 bits sur logiciel, Metal et OpenGL : zoom, déplacement, rotation, cadre hors champ, ouverture dégénérée et annulation.
- [x] Cadre, grille et poignées de recadrage classique/perspective dans le graphe de scène GPU via Qt Quick Shapes CurveRenderer et rectangles conservés, sans Canvas de guides. Repli raster logiciel/anciens kits Qt ; tests des pixels, zoom, déplacement, rotation, annulation et absence de repeint raster, avec différences de lissage explicitement bornées.
- [x] Trait et repères circulaires de la règle sur GPU, sans Canvas ni texture intermédiaire ; composants communs de vue/CurveRenderer avec le recadrage. Repli logiciel/Qt ancien, lectures Informations inchangées et tests des points, zooms, défilement, rotation, mesure 16K, masquage et absence de repeints raster sur GPU.
- [x] Anneau de pipette procédural sur GPU : deux couleurs et liserés sans Canvas ni texture source, mises à jour par paramètres, transparence et opacité héritée préservées. Repli logiciel borné et repli sur erreur de shader ; comparaison à l’ancien rendu pour les couleurs, alpha, déplacement, redimensionnement, rotation et masquage sur logiciel/Metal/OpenGL, sans repeint raster sur GPU.
- [x] Contour de pinceau procédural sur GPU, sans Canvas ni texture même pendant un déplacement coupé par la vue. Angle, aplatissement, traits contrastés, signes +/− et opacité conservés ; repli logiciel borné. Comparaisons à l’ancien dessin sur logiciel/Metal/OpenGL : formes, rotation, clipping, 32 000 pixels, diamètre d’un pixel, aplatissement minimal, coordonnées fractionnaires et réglages modifiés pendant le masquage.
- [x] Tracé principal de la Plume conservé sur GPU via CurveRenderer : deux traits sans Canvas ni texture, cache au survol et préparation des cubiques en quadratiques avec erreur bornée à 0,01 pixel logique. Emprise visible découpée, repli logiciel/Qt ancien, poignées et segment provisoire inchangés. Comparaisons 8/16 bits sur logiciel/Metal/OpenGL : édition, Annuler, zoom 64×, défilement fractionnaire, rotation, fermeture, ancre seule, angles, croisements et réactivation.
- [x] Cadre provisoire des tranches sur GPU, séparé du fond numéroté : création, déplacement et redimensionnement sans repeindre les tranches fixes. Révision du cache natif filtrant les notifications de brouillon dans le canevas et les options ; buffer et snapshots réutilisés. Repli logiciel/Qt ancien et comparaisons 8/16 bits sur logiciel/Metal/OpenGL, avec annulation, validation, zoom, défilement, rotation et compteurs de repeint.
- [x] Dessin GPU du panneau Histogramme : zones RVB et luminance via CurveRenderer, couche de composition bornée conservant alphas et opacité, projection fractionnaire compatible avec le Canvas et contour de luminance préparé pour conserver les pics fins. Contexte Canvas absent sur le chemin matériel ; repli logiciel et statistiques CPU conservés. Tests logiciel/Metal/OpenGL des couleurs, pics, redimensionnement, masquage et transfert entre fenêtres, plus le smoke test du panneau.
- [x] Règles du canevas bornées aux graduations visibles : traits et contours des chiffres conservés sur GPU pour les bandes de dimensions entières, cache de 128 étiquettes et suppression des repeints liés aux seules modifications de pixels. Repli Canvas borné pour le logiciel, Qt sans CurveRenderer et les dimensions fractionnaires, avec nettoyage complet des bords. Comparaisons logiciel/Metal/OpenGL du zoom, défilement, orientation, opacité et masquage ; test des grandes coordonnées et liaisons réelles du viewport.
- [x] Graphique Seuil conservé sur GPU : histogramme et grille indépendants de la valeur, bande noir/blanc et repère mis à jour sans repeint Canvas sur le chemin éligible. Repli logiciel/Qt ancien/dimensions fractionnaires conservant l’échantillonnage. Tests logiciel/Metal/OpenGL des extrêmes, pics, redimensionnement, rotation, opacité et masquage ; contrôles de géométrie conservée et geste réel avec une seule annulation.
- [x] Graphiques Niveaux sur GPU : grille et géométrie d’histogramme indépendantes des seules valeurs noir/gamma/blanc, dégradé de sortie conservé et cinq triangles repositionnés sans repeint raster sur le chemin éligible. Statistiques mises à jour normalement, repli logiciel/Qt ancien/dimensions fractionnaires et nettoyage du tampon transparent au retour GPU. Tests logiciel/Metal/OpenGL des extrêmes, superpositions, taille, rotation, opacité, masquage et transitions ; gestes réels des cinq poignées avec une annulation par geste.
- [x] Graphique Courbes sur GPU et interpolation en cache : snapshot points/pentes et 256 échantillons reconstruits une fois par nouvelle liste, sans réévaluation au changement de point actif, d’histogramme ou de taille. Grille, histogramme, diagonale, contour de transfert et poignées conservés ; contour préparé avec QPainterPathStroker et couche MSAA bornée. Repli logiciel/Qt ancien/dimensions fractionnaires et masquage sans repeint. Comparaisons numériques à 1e-9 et visuelles logiciel/Metal/OpenGL, dont pics serrés et listes dégénérées ; gestes réels d’ajout, déplacement, suppression et annulation.
- [x] Statistiques d’histogramme CPU : collecte de la grille native avant conversion Qt en ARGB32, positions/alpha/arrondis inchangés et deux résultats normalisés conservés par document sans retenir l’image. Réutilisation par identité réelle des pixels affichés ; tests exacts sur les formats QImage, transparence, palettes et 24 Mpx 16 bits, invalidation par retouche/couche affichée/Annuler/Rétablir/nouveau document. Mesure locale des deux calculs : environ 31→3 ms, hors composition et dessin.
- [x] Roue du Transfert de dégradé : spectre teinte/saturation conservé entre les changements de couleurs et de poignée, voile de luminosité procédural sur GPU actualisé par son opacité seule. Repli Canvas logiciel/dimensions fractionnaires/erreur de shader ; comparaisons visuelles logiciel/Metal/OpenGL, transitions de repli et gestes réels de couleur/luminosité avec une annulation par geste.
- [x] Aperçu de pointe des paramètres du pinceau isolé et procédural sur GPU : rectangle conservé, au plus 17 empreintes évaluées par fragment et absence de texture source. Propriétés visuelles indépendantes des notifications générales ; repli Canvas pour pointes fines, superpositions denses, tailles fractionnaires et logiciel. Comparaisons logiciel/Metal/OpenGL des réglages, limites et transitions ; contrôle du dialogue réel, absence de repeint caché et annulation.
- [x] Guides unis du lasso et repères du polygonal sur GPU : points, enveloppe et préfixe fixes en cache au survol, chemins de repères conservés et couche de composition limitée à la vue. Repli Canvas pour logiciel/Qt ancien, candidats combinés et onglets presque superposés ; marges complètes pour les pointes et remise à zéro du trait après pointillés. Tests 8/16 bits logiciel/Metal/OpenGL jusqu’à 64×, rotation, opacité, transitions, masquage, sortie de vue et gestes réels sans modifier les pixels ni l’historique au survol.
- [x] Ancres et tangentes de la Plume sur GPU : primitives visibles ordonnées et conservées au survol, lignes traversantes gardées lorsque leurs extrémités sont hors champ, couche de composition limitée à la vue et opacité après superposition. Repli Canvas logiciel/Qt ancien sans préparation SVG ; aucun dessin caché ou entièrement hors champ. Tests 8/16 bits logiciel/Metal/OpenGL, édition et Annuler, zoom 64×, contrôles proches, bords, vecteurs presque nuls, masquage et vrai glisser d’ancre. Les interactions de la Plume et son segment provisoire restent inchangés.
- [x] Segment provisoire de la Plume sur GPU pour les courbes entièrement visibles : contour pointillé préparé par le moteur de trait Qt à la résolution de l’écran, remplissage CurveRenderer et opacité composée sur une couche bornée. Objet réutilisé au survol ; enveloppe contrôlée par la vue stable et polygone de contrôle limité à 8 192 pixels logiques. Repli Canvas pour les courbes coupées/longues, le logiciel et Qt ancien ; aucun dessin nul, caché ou hors champ. Comparaisons 8/16 bits logiciel/Metal/OpenGL, facteurs d’écran 1×/2× sur Metal, retours en arrière, faible opacité, transitions et gestes réels avec Annuler.
- [x] Pointillés des candidats combinés du lasso libre sur GPU : deux passes noir/blanc, géométrie native bornée et conservée pendant la phase, extrémités plates à la fermeture et opacité après composition. Repli Canvas pour angles aigus, tracés dégénérés/hors budget, logiciel et guides polygonaux combinés. Comparaison 8/16 bits, phases limites, zoom 64×, croisements, rotation, masquage et compteurs de préparation/transfert.
- [x] Arrêt du rendu des candidats de lasso entourant la vue sans segment visible : maillage vide distingué du repli logiciel, couche de composition désactivée et aucune demande de rendu pour les phases suivantes. Objet et phase conservés au retour sur un bord ; contrôles des pixels, de la couche et des compteurs en 8/16 bits.
- [x] Préparation GUI courte pour les phases des sélections GPU : snapshots et cache de tuiles/masques/LRU réutilisés sans reparcours. Les demandes complètes gardent la priorité, y compris le budget mémoire sans pixels modifiés. Compteurs sur contours généraux, rectangles/ellipses et lassos ; contrôle des pixels et de l’éviction lors de demandes image/budget coalescées.
- [x] Animation de sélection suspendue quand la fenêtre est masquée ou réduite, puis reprise au retour : pas de nouvelles phases inutiles pour le canevas hors écran.
- [x] Cache des textures : tri LRU évité lorsque le budget est épuisé ou que toutes les tuiles candidates tiennent ; tri limité aux candidates sinon. Les tuiles visibles restent prioritaires et la politique de rétention est inchangée.
- [x] Préparation des contours de sélection GPU : réutilisation de la normale déjà calculée pour chaque segment et ses raccords, avec allocation des points limitée au contour. Géométrie et repli inchangés.
- [x] Préparation des contours GPU : élimination anticipée des tracés entièrement hors de la vue, avant conversion en polygones, en conservant les segments sur le bord et les courbes visibles.
- [x] Surimpressions Suppression et Pièce : rejet des régions entièrement hors de la vue avant parcours de leurs rectangles, avec marge de précision pour les coordonnées aux limites.
- [x] Contours GPU sans segment visible : les changements de phase ne réveillent plus le graphe de scène ; la phase courante est appliquée dès le retour du contour dans la vue.
- [x] Flou de boîte 16 bits : passe verticale par bandes de colonnes contiguës, avec sommes locales et résultat identique au calcul précédent ; ce chemin sert aussi au flou gaussien et à d’autres effets.
- [x] Composition des masques de calque : grandes zones traitées par blocs de lignes en parallèle, avec conservation des arrondis 8/16 bits et traitement séquentiel dans les tâches déjà sur le pool.
- [x] Calques écrêtés successifs : couverture de la base calculée une fois par composition et réutilisée jusqu’au changement de base, avec invalidation aux séparations et masquages.
- [x] Couverture d’écrêtage : extraction directe de l’alpha des formats natifs 8/16 bits, sans conversion temporaire de tous les canaux couleur ; quantification identique à Qt.
- [x] Masques 8 bits : table de couverture par densité et multiplication entière exacte des canaux prémultipliés, avec conservation des arrondis du calcul flottant.
- [x] Composition : rejet anticipé des calques à opacité nulle et des calques écrêtés sans base visible ; les effets à opacité nulle ne forcent plus une recomposition intégrale ni une marge de filtre.
- [x] Masques sans effet : densité nulle sans copie des pixels, et calques masqués par un masque compact entièrement noir écartés avant les filtres et les marges de composition.
- [x] Estimation du budget graphique : coût par calque agrégé par mode de stockage, puis appliqué aux tuiles visibles. La prévision est conservée tant que le contenu et les tuiles visibles restent identiques. Les plans natifs uniformes utilisent un seul texel pour leurs entrées couleur ou masque.
- [x] Paires de fréquences intactes affichées par la pile GPU à partir d’un seul plan source, avec repli CPU dès qu’un réglage de la paire change la composition.
- [x] Visibilité des fonds et des masques uniformes lue sur leur pixel natif. Les calques écrêtés sans base visible sont écartés avant la validation GPU et les piles vides gardent leur sortie transparente.
- [x] Réglages analytiques de la pile GPU sans texture de table factice. Buffers de paramètres réutilisés entre retouches et shaders précompilés désérialisés une fois pour la pile, les filtres spatiaux et la pyramide d’affichage.
- [x] Ajout de bruit sur GPU en 8/16 bits sur fonds opaques et translucides. Le motif utilise les coordonnées du document et l’ordre des canaux de la référence CPU. Comparaisons du rendu écran avec alpha variable, masque, opacité et raccords de tuiles.
- [x] Mosaïque sur GPU en 8/16 bits. Une table compacte réutilise la grille d’échantillonnage Qt sur les deux axes, avec le halo nécessaire aux raccords. Le shader conserve l’alpha du pixel échantillonné.
- [x] Changements de vue et de budget sans reconstruire la description de la pile GPU. Le redimensionnement d’une table de réglage conserve les cibles partagées et les textures couleur compatibles.
- [x] Entrées GPU réutilisées limitées à un texel quand tous leurs plans sont uniformes. En mode à entrées conservées, les aplats et masques uniformes identiques sont transférés et budgétés une fois par canevas. Allocation d’une tuile à la première retouche et retour au format compact avec Annuler.
- [x] Lots de transfert GPU créés à la demande. En mode partagé, la dernière passe écrit directement dans la texture native affichée. Les changements de géométrie réutilisent le rendu des tuiles sans recalculer leurs signatures. Les paramètres et tables sont encodés une fois par révision de la pile. Le bloc tonal préparé sert à la fois à la signature et au tampon GPU de chaque tuile.
- [x] Signatures régionales réutilisées entre composition et transfert. Les tuiles couleur et masque 8 bits transmises en entier utilisent directement leur image de préparation. Les tuiles d’un canevas partagent un tampon de sommets et un échantillonneur.
- [x] Tables de réglage et de fusion immuables transférées et budgétées une fois par canevas en mode à entrées conservées. Les tuiles partagent les pipelines compatibles et les retouches de paramètres conservent les liaisons GPU quand les textures restent les mêmes.
- [x] Buffers et liaisons des passes de flou, de netteté et de masque progressif réutilisés dans les espaces de travail privés. Les paramètres inchangés des passes conservées ne sont plus retransmis au GPU.
- [x] Cibles spatiales allouées à la demande. Le flou directionnel utilise une seule cible et chaque cible est libérée quand les filtres actifs cessent de l’utiliser. Les passes spatiales et les masques progressifs partagent la géométrie, l’échantillonneur et les pipelines compatibles sur tout le canevas.
- [x] Composition GPU à sortie couleur unique pour les piles sans calque écrêté. Deux textures R8 de couverture allouées à la demande, avec estimation du budget adaptée aux deux variantes.
- [x] Masques progressifs sur cibles à un canal : R32F pour les sommes de la passe horizontale lorsque le backend le permet, R8 pour la couverture finale. La prévision couvre le repli RGBA32F de la première passe. Le dernier résultat peut être réutilisé sans nouvelle convolution lorsque seuls l’opacité ou la densité changent.
- [x] Réduction GPU aux faibles niveaux de zoom avec paramètres transférés une fois par niveau, géométrie partagée et pipelines réutilisés entre tuiles et niveaux compatibles. Une seule cible horizontale par dimension, même avec des accumulateurs privés. Elle est libérée avec le dernier niveau qui l’utilise.
- [ ] Étendre les filtres et la composition sur GPU aux autres réglages, masques et mélanges ; autres surimpressions par shaders et budget global de VRAM. Mesurer les gains et la fidélité sur chaque backend matériel. Réduction du bruit avancée.

Étape en développement après v0.16 : cadres et poignées de tranches en chemins GPU
conservés ; chiffres en petits caches de 49 × 25 pixels logiques, à la phase de pixels
du Canvas historique. Modèle visible conservé au petit défilement, composition de
l’opacité et destruction des surfaces au masquage/changement de chemin. Repli pour
les écrans fractionnaires, Qt ancien, logiciel et plus de 128 tranches visibles.
Comparaisons 8/16 bits, zoom 64×, poignées superposées et réutilisation des nœuds.

## 3. Sélections et masques à compléter

Plage de couleurs, Contour progressif, Frange, Lisser, Dilater, Contracter, Resélectionner et l’affichage des contours sont livrés. Les étapes restantes sont :

- [ ] Anticrénelage fractionnaire des outils, Mode Masque et chargement depuis un calque/masque/couche.
- [ ] Tons chair/visages, hors gamut CMJN, réglages tonals détaillés, aperçus et préréglages Adobe complémentaires ; Étendre/Généraliser selon la tolérance de la Baguette magique.
- [ ] Zone de mise au point, Sujet, Ciel et espace Sélectionner et masquer, avec validation des détails fins et calculs asynchrones.
- [ ] Transformation du contour indépendante des pixels ; mémorisation/récupération dans des couches alpha persistantes.
- [ ] Aucun calque actif, recherche et isolation par filtrage du panneau Calques.
- [ ] Paramètres de contour progressif et d’anticrénelage lors de la conversion Plume → sélection ; le contour progressif peut déjà être appliqué après conversion.

Les paramètres et écarts avec Adobe sont détaillés dans [docs/SELECTIONS.md](docs/SELECTIONS.md). Les sélections de pixels, la sélection de calques, les masques de calque et les couches alpha sont des opérations distinctes.

## 4. Outils encore NYI

Les **15 outils NYI** du catalogue sont tous recensés ci-dessous et désactivés dans l’interface. Ils restent des fonctions futures, sans calendrier de livraison.

| Famille | Outils restant à implémenter |
|---|---|
| Déplacement et sélection | Plan de travail, pinceau de sélection, lasso magnétique, sélection d’objet |
| Recadrage et mise en page | Cadre |
| Mesure et annotation | Annotation, comptage |
| Gomme et réglage local | Gomme d’arrière-plan, pinceau de réglage |
| Tracés | Plume libre, plume de courbure |
| Texte et formes | Texte vertical, masque de texte horizontal, masque de texte vertical, forme personnalisée |

Améliorations des outils déjà disponibles :

- [x] Texte éditable ponctuel/paragraphe, double-clic sur la vignette T, saisie/curseur/sélection sur l’image, formats mixtes, familles/styles, couleur, interligne, approche, crénage métrique, échelle horizontale, ligne de base, capitales et indices/exposants ; alignements, retraits et espacements de paragraphes. Annulation par session, projets v9, transformations conservées et pixellisation explicite. [Inventaire Adobe et limites](docs/TEXT_TOOL.md).
- [ ] Texte sur tracé, déformations, texte vertical/masques, options OpenType et polices variables, styles réutilisables, crénage optique, lissages Adobe, typographie CJK détaillée, césure, correction/recherche et Adobe Fonts.
- [x] Barre des sélections géométriques et lassos : modes de combinaison illustrés, contour progressif préalable, anticrénelage effectif, styles Normal/Rapport fixe/Taille fixe et permutation des dimensions. Couverture 8/16 bits, calcul asynchrone annulable et une seule étape par geste ; espace Sélectionner et masquer encore NYI.
- [x] Première révision de la Plume depuis la documentation Adobe : reprise par glisser, poignée sortante indépendante au dernier point, flèches noire/blanche (A), sélection par cadre et déplacement de points/segments/tracés, curseurs distincts, désélection sans effacement. Panneau Tracés ancrable/flottant avec vignettes, renommage, duplication, ordre et actions de sélection/remplissage/contour ; projets avec tracés vides en v10. [Comportements et limites](docs/PEN_TOOL.md).
- [x] Mode Forme de la Plume : calque créé au premier point, géométrie et styles conservés, fond/contour unis ou absents, épaisseur et tirets, réédition par les points ou les propriétés, déplacement, masques raster, duplication, Annuler et pixellisation explicite. Conversion explicite Tracé → Forme en place, sans doublon du tracé ; couleurs réaffichées à la sélection, remplissage lié aux poignées et distinction du remplissage raster dans le panneau. Choix Aucun/couleur, épaisseur décimale et dimensions liées. Caches natifs 8/16 bits par tuiles, affichage GPU et projets v11.
- [ ] Compléter les tracés : composants multiples et jonctions, opérations booléennes, tracé de travail temporaire, multi-sélection de tracés, duplication par Alt-glisser et réordonnancement par glisser ; sélection → tracé et composants multiples des masques vectoriels. Les dégradés/motifs et alignements avancés des contours de forme restent à faire. La découpe indépendante d’un segment et les options de conversion restent à faire.
- [ ] Conservation non destructive des pixels hors canevas, rotation/redressement du cadrage, résolution physique et presets personnalisés ; propriétés web/HTML et export des tranches automatiques.
- [ ] Compléter le déplacement multicalques, l’auto-sélection sous la souris, les transformations et le sélecteur contextuel de pointes/motifs.
- [ ] Tolérance et options supplémentaires du remplissage contigu ; modes plus avancés de réparation et de pièce.
- [ ] Transformation au dépôt pour le déplacement basé sur le contenu ; meilleurs raccords sur les fonds complexes.
- [ ] Pression/inclinaison du stylet, pointes texturées, dynamiques de brosse et bibliothèque de motifs/pointes.
- [ ] Rectangle de navigation interactif dans le Navigateur.

## 5. Édition non destructive et couleur

- [ ] Compléter les commandes NYI des calques : placement direct tout en haut/en bas, verrouillage des pixels transparents, étiquettes de couleur, pixellisation et options de fusion avancées.
- [ ] Compléter les commandes NYI des masques : appliquer le masque, associer/dissocier, charger comme sélection et Sélectionner et masquer.
- [ ] Compléter les masques vectoriels (dossiers, composants multiples), effets de calque et objets dynamiques.
- [ ] Transformation libre, perspective interactive des calques et formes vectorielles.
- [ ] Dégradé non destructif avec calque de remplissage et poignées ; l’outil actuel reste un dégradé raster classique.
- [ ] Fidélité colorimétrique des réglages et jeux d’images de référence ; les traitements actuels sont des implémentations sRGB simplifiées.
- [ ] Lectures supplémentaires dans Informations : espaces colorimétriques, opacité, avant/après et unités de mesure configurables.
- [ ] Gestion ICC complète, épreuvage écran/impression, RVB 32 bits, CMJN, tons directs et HDR.
- [x] Ouverture/import RAW asynchrone via LibRaw ; stockage, réglages photo, projets et PNG en 16 bits par canal.
- [ ] Porter les outils de réparation, Doigt, Pinceau mélangeur, Remplacement de couleur et formes d’historique en 16 bits.
- [ ] Développement RAW réglable, corrections optiques et réduction du bruit ; validation sur des fichiers de boîtiers réels.

### Réglages directs et masques vectoriels — en préparation

- [x] Réglages directs via les éditeurs existants et menu Filtre, sur les pixels
  ou le masque de fusion actif, aperçu réduit asynchrone, Annuler/Appliquer et
  une étape d’historique ; sélection respectée et halo local des filtres spatiaux.
- [x] Masque vectoriel depuis un tracé, vignette indépendante, édition des ancres
  et poignées, densité/contour progressif, activation et pixellisation explicite ;
  coexistence avec le masque peint, transformations et sauvegarde du contour.
- [ ] Masques vectoriels à composants multiples et opérations booléennes,
  masques de dossiers, dissociation du déplacement et affichage GPU analytique.
- [ ] Commandes automatiques Tonalité/Contraste/Couleur et Désaturer dédiées.

[Guide et limites](docs/MASK_ADJUSTMENTS.md).

## 6. Commandes d’édition et d’affichage encore NYI

Ces manques sont déjà indiqués dans le catalogue des raccourcis, même lorsque leur panneau ou une commande voisine existe :

- [ ] Couper/copier les pixels du calque actif, nouveau calque par copie/couper de sélection, Coller sur place et Coller dans la sélection. Aujourd’hui Copier utilise la composition visible, Coller crée un calque et Dupliquer copie le calque entier.
- [ ] Taille de la zone de travail indépendante du redimensionnement de l’image ; rétablissement du fichier enregistré.
- [x] Réglages directs Niveaux, Courbes, Couleur, Balance des couleurs et Noir et blanc. Les commandes automatiques dédiées et Désaturer restent à faire.
- [ ] Répéter le dernier filtre et atténuer la dernière opération ; Fluidité et filtre de développement RAW.
- [ ] Dialogue Remplir complet (motif, historique, contenu) ; le sélecteur de couleur et les remplissages premier/arrière-plan sont déjà disponibles.
- [ ] Grille de travail, repères, magnétisme, Extras, modes d’écran et masquage de la barre supérieure des options. Les règles, Tab/Maj+Tab et l’affichage du contour de sélection existent déjà.
- [ ] Gestes contextuels complémentaires : source du tampon décalable/rotative/redimensionnable, rapporteur de la règle, réglage Noir et blanc sur l’image, raccourcis détaillés des points de Courbes et raccourcis typographiques avancés. L’inventaire par geste est dans [docs/SHORTCUTS.md](docs/SHORTCUTS.md).

## 7. Interopérabilité et extensibilité

- [ ] Import/export PSD/PSB sur un sous-ensemble explicitement testé : structure, pixels, masques et modes de fusion.
- [ ] Métadonnées EXIF/XMP et Informations du fichier ; impression, export web avancé et traitement par lots.
- [ ] API de plugins versionnée, scripting et actions.
- [ ] Intégration facultative de modèles locaux après validation des licences, des ressources nécessaires et de la qualité réelle des masques/rendus.
- [ ] Comparaisons de rendu et mesures de performance avant toute revendication de compatibilité avec une version précise de Photoshop.

Les fonctions déjà réalisées restent détaillées dans [README.md](README.md) et [README.en.md](README.en.md). Les sections historiques et les cases cochées décrivent les fonctions livrées ; les contrôles CI attestent les scénarios testés, pas une validation exhaustive de tous les usages. Les cases non cochées sont des travaux restants.
