# Validation

## Références des deux chemins CPU — 3 novembre 2026

Les références relevées sur Accelerate ne sont pas toutes celles du flou de boîte
portable : leurs arrondis étaient déjà différents. Six digests de la série ont
été vérifiés sur le chemin portable, puis comparés à une variante de référence
qui prélève chaque source composée sur l’image intégrale et utilise l’ancien
flou global des masques. Les pixels régionaux sont identiques à cette référence.
Les comparaisons de cache et d’historique restent également actives dans les
suites normales ; aucun test n’est ignoré pour cette différence de backend.

| Cas | SHA-256 du chemin portable |
|---|---|
| Tampon | `4e9144d74453c0546afde97b06f273757033eb93ae831eee49a4219fe999da34` |
| Doigt | `252c539838bd1b2d888a71c3d725c1890c7dacc9dc7947497883c6c8d0825ad4` |
| Mélangeur | `96907b36823647dd022699c833614511df6f71f5d3e8ff899dc25ca4e2297d5c` |
| Copie sélectionnée | `fba56b9c727d606f5ac31b5831999b03a4e9e7314f4c15672df73f92376bfef9` |
| Sélection rapide | `668592579e0d8efb126cf66b5c9357783a185dbc78d05c5ec7eb9e3124b1faa8` |
| Sources de fréquences | `36170c0ebf9a9e6e2ab623ae60a5930c8c32f18255c09337de74f44c09e79d30` |

`PHOTOCRAP_ENABLE_ACCELERATE` vaut ON par défaut ; sur les plateformes sans
Accelerate, le chemin portable et ses références restent toujours utilisés.
Sur Mac, OFF permet de reconstruire et contrôler ce chemin entièrement :

```sh
cmake -S . -B build-portable -DCMAKE_BUILD_TYPE=Release -DPHOTOCRAP_ENABLE_ACCELERATE=OFF
cmake --build build-portable --parallel 8
ctest --test-dir build-portable -E '^macos_' --parallel 2 --output-on-failure
```

Résultats locaux Qt 6.12 : 50/50 suites avec Accelerate, puis 50/50 sans
Accelerate ; les suites Document/Sélection/QML ont aussi été relancées après
l’ajout du contrôle de copie progressive. Dix cas ciblés de références et de
copie passent sous Qt 6.8. Les essais Cocoa/Metal des opérations et de la pile
complètent cette validation.

Cela vérifie le code CPU portable sur ce Mac, sans remplacer les essais physiques
Linux/Windows, Vulkan/Direct3D et VRAM encore ouverts.


## Source native partagée des fréquences — 3 novembre 2026

La référence avant modification couvre 8 scénarios en 8/16 bits, masque absent,
activé ou désactivé. Bandes et composite conservent le digest :
`05f6ee8bb2a0fc1c457699d9babbf0ed07a0c7aee79b96d42f96b1266e17250d`.
Retoucher ensuite le calque d’origine laisse les deux bandes inchangées. Le test
mémoire vérifie qu’une source native non masquée ne rajoute aucun bloc de tuile,
y compris après Annuler/Rétablir.

Sonde Qt 6.12 locale, dégradé dense 24 Mpx/16 bits : création 94,5 ms → 0,097 ms ;
192 000 000 octets (183 Mio) de tuiles supplémentaires → zéro. Ce sont les blocs
natifs suivis par le document, pas une mesure de RSS total. Les sources masquées,
composées et compactes gardent leur préparation complète actuelle ; le rayon et
les algorithmes de séparation restent inchangés.

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  PHOTOCRAP_FREQUENCY_SOURCE_PROBE=1 build/photocrap_tests \
  frequencySourceNativeDigest frequencySourcesShareUnmaskedNativeTiles largeFrequencySourceProbe
```


## Sources régionales des correcteurs — 3 novembre 2026

Références avant modification : 36 gestes du Correcteur et 12 du Correcteur
localisé. Les cas couvrent source active/inférieurs/tous selon l’outil,
exclusion des réglages, modes normal/remplacement, textures, bords du document,
frontières de tuiles, chevauchement et annulation. Les digests restent identiques :

- Correcteur : `8e6a6d00405dbb88b78e48f55a9fa35b7d529f65372aa6a904618a1f622649db`.
- Correcteur localisé : `6801707827136d7d089a459db3c844ad00f41da8171196b0b07687a2b8897687`.

Sonde Qt 6.12 locale, petite réparation sur calque vide en 24 Mpx/8 bits :
Correcteur 20,4 ms → 0,68 ms ; Correcteur localisé 12,8 ms → 0,81 ms. Une seule
étape d’historique est créée. Aucun seuil de durée n’est imposé. Le modèle
8 bits des algorithmes reste inchangé ; les piles non locales gardent leur rendu
complet paresseux.

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  PHOTOCRAP_COMPOSITE_HEALING_PROBE=1 build/photocrap_tests \
  compositedHealingStrokeDigest largeCompositeHealingProbe
```


## Copie régionale des sélections — 3 novembre 2026

Référence avant modification sur 24 copies : 8/16 bits, rectangle/ellipse,
pile simple/flou/bruit, transparence et fusion. Une demande de contour progressif
encore en file est annulée lors du changement de source, comme auparavant.
Le digest des pixels natifs du presse-papiers reste identique :
`c775db53fad8e01d5fbf270fbed9745d586c59884cd1aac2dd050eed1bdcb77c`.
La visualisation d’un canal n’altère pas les couleurs copiées. L’historique est
inchangé, le cache d’affichage reste intact pour les piles compatibles et Copier
sans sélection conserve le composite complet. Tests exécutés avec le presse-papiers
virtuel du backend offscreen, sans remplacer celui de la session utilisateur.
Un contrôle distinct attend réellement la fin du contour progressif et vérifie la
présence de valeurs intermédiaires dans son masque : huit copies 8/16 bits,
rectangle/ellipse et flou/bruit sont ensuite comparées au prélèvement sur l’image
intégrale, avec leur forme et leur couverture exactes.

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software build/photocrap_tests \
  selectedClipboardDigest featheredClipboardMatchesWholeImageReference
```


## Contours progressifs de masque régionaux — 3 novembre 2026

La référence native avant modification couvre 16 piles : 8/16 bits, rayons
1/7/40/100 et densités 27/100 %, masque peint aux bords et aux frontières de
tuiles, couleur translucide, opacité et mode de fusion. Le rendu après retouche
locale est égal au rendu intégral ; les prélèvements régionaux correspondent aussi
aux pixels intégraux. Le digest des rendus reste identique :
`06919da73b9e9e92e8328a5f41ef123953e032e7290fe75e88a73b6d37f15c44`.

Sonde locale Qt 6.12, 24 Mpx/16 bits, contour de rayon 12 : une lecture 101² passe
de 297 ms à 0,45 ms. Digest inchangé :
`71da737a76aaf3e21ed9b09234dc56dfab99f380cdb2dab345996b1531729417`.
Les tests du compositeur contrôlent maintenant que ce cas conserve le cache
d’affichage. Aucun seuil de durée n’est imposé aux tests.

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  PHOTOCRAP_FEATHER_REGION_PROBE=1 build/photocrap_tests \
  featheredMasksRegionalDigest largeFeatheredMaskRegionProbe \
  compositeRegionMatchesFullRender compositeUpdateBounds
```


## Recherche régionale de Sélection rapide — 3 novembre 2026

Référence relevée avant modification sur 96 gestes : documents 8/16 bits,
calque actif/tous les calques, amélioration des bords activée/désactivée,
remplacement/ajout/soustraction/intersection, pile simple/flou/bruit. Les gestes
passent par le bord du document et une frontière de tuiles ; leur annulation
restaure région et masque. Le digest des masques reste identique :
`7376129ff7bec138618e931011e97cf886c57a1a49798aec0407213a28df11ae`.
Les fonds compacts blancs/transparents et les documents d’un pixel sont testés
séparément, avec cache d’affichage inchangé.

Sonde locale Qt 6.12 sur 24 Mpx/16 bits : calque actif, 40,2 ms → 0,078 ms ;
tous les calques, 59,8 ms → 0,098 ms. La région résultante reste 25 × 25 pixels.
Aucun seuil de durée n’est imposé aux tests. Une brosse plus grande étend toujours
la fenêtre de recherche ; les piles non locales peuvent demander un repli complet.

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  PHOTOCRAP_QUICK_SELECTION_PROBE=1 build/photocrap_tests \
  quickSelectionRegionalDigest quickSelectionHandlesCompactPlanesWithoutRebuildingDisplay \
  largeQuickSelectionProbe
```


## Pipettes de réglages régionales — 3 novembre 2026

Les lectures de couleur des réglages et les pipettes noir/gris/blanc de Courbes
utilisent un pixel du composite régional. Le test compare directement la couleur
native du rendu complet en 8/16 bits, avec alpha, bords de tuiles et coordonnées
hors image bornées comme auparavant. Le cache d’affichage reste intact et une
lecture simple ne crée aucun historique. Les tests existants de neutralisation
des courbes restent la référence du réglage appliqué. Les piles non locales
conservent le même repli complet que les autres lecteurs régionaux.

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software build/photocrap_tests \
  adjustmentPipettesPreserveNativeSamplesAndDisplayCache \
  curveEyedroppersSampleColorsAndNeutralizeGray
```


## Source et prélèvement du Pinceau mélangeur — 3 novembre 2026

Les 54 gestes enregistrés avant modification conservent leur digest :
`08863a90f51429679357eab832355d59906e10a8ca22e9d604d27872a0450026`.
Ils couvrent pixels 8 bits, masques 8/16 bits, sources active/composite,
chevauchements, frontières et annulation. Le test de prélèvement de couleur unie
compare 16 lectures natives actives/composées, avec transparence et tuiles :
`aac30ae02e3b14043f16fb31f3aa180f76f08a46e25b55eb393cc260cf3d8b5a`.
L’historique et le cache d’affichage restent inchangés pendant ces prélèvements.

Sonde locale Qt 6.12 : le petit geste sur 24 Mpx/8 bits passe de 12,3 ms à 0,73 ms,
sans changement du pixel échantillonné. La lecture texturée est également couverte
par le digest des gestes. Les replis non locaux restent nécessaires ; aucun seuil
de durée n’est imposé aux tests.

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  PHOTOCRAP_COMPOSITE_PICKUP_PROBE=1 build/photocrap_tests \
  compositedPickupStrokeDigest largeCompositePickupProbe mixerSolidPickingDigest
```


## Source composée du Doigt — 3 novembre 2026

Référence enregistrée avant modification sur 54 gestes par outil : pixels
8 bits, masques de documents 8/16 bits, source active/tous les calques, pile
simple/flou/bruit, bords du document et frontières de tuiles. Traits fractionnaires,
chevauchement, annulation du geste et Annuler/Rétablir sont contrôlés. Les gestes
sans modification ne consomment pas d’étape d’historique.

Digest du Doigt inchangé :
`78315359d8def1e00cf94dd46b784e1a213f50482a70606d436e894bef7d4e15`.
Le test fige aussi le mélangeur, afin de détecter un effet indirect du moteur
commun. Son optimisation vient séparément.

Sonde locale Qt 6.12, 24 Mpx/8 bits : le petit geste du Doigt « tous les calques »
passe de 19,5 ms à 0,64 ms, avec le même pixel prélevé. Aucun seuil de vitesse
n’est imposé. La suite Document contient 352 cas réussis et quatre sondes ignorées
lors de ce passage avec la sonde de prise de couleur activée ; contrôle Qt 6.8
et essai natif Cocoa/Metal effectués.

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  PHOTOCRAP_COMPOSITE_PICKUP_PROBE=1 build/photocrap_tests \
  compositedPickupStrokeDigest largeCompositePickupProbe
```


## Sources composées du tampon — 3 novembre 2026

La référence avant modification couvre 36 traits : 8/16 bits, trois modes de
source, pixels/masque, pile simple, flou et bruit. Les gestes fractionnaires
recouvrent leur source et traversent les frontières de tuiles. Annuler, Rétablir
et annuler le geste sont vérifiés. Le digest natif sans padding reste identique :
`13537ea6c2f1f78417bfe686cc085a7dd3b969b2061ae2af6830aacac6715d62`.

Sonde locale Qt 6.12, 24 Mpx/16 bits, petite retouche sur un calque vide :
« actif et inférieurs » passe de 27,2 ms à 0,64 ms ; « tous les calques » de
14,3 ms à 0,62 ms. Les piles à effets non locaux gardent leur repli complet ;
aucun seuil de durée n’est imposé aux tests.

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  PHOTOCRAP_COMPOSITE_CLONE_PROBE=1 build/photocrap_tests \
  compositeCloneStrokeDigest largeCompositeCloneProbe
```


## Composition régionale pour la pipette — 3 novembre 2026

Le moteur CPU est commun au rendu complet et aux lectures régionales. La
comparaison est exacte en 8/16 bits sur les 27 modes de fusion, les cinq filtres
spatiaux, leurs halos cumulés, un réglage Lumière, les masques et leur densité,
les groupes visibles/masqués et l’écrêtage. Les bords du document et de tuiles
sont inclus ; une lecture régionale compatible ne remplace pas le cache affiché
et ne modifie pas l’historique. Les replis complets des effets à coordonnées
absolues, fréquences et contours progressifs sont aussi comparés.

Sonde Qt 6.12 locale : 24 Mpx/16 bits, trait rouge et réglage Négatif, cache froid.
Une lecture 101 × 101 a pris 0,38 ms, contre 248 ms pour le rendu entier de la même
pile ; pixels identiques. Cette mesure n’est pas un engagement de temps réel pour
toutes les piles et n’impose aucun seuil aux tests.

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  PHOTOCRAP_COMPOSITE_REGION_PROBE=1 build/photocrap_tests \
  compositeRegionMatchesFullRender compositeRegionPreservesGroupsAndClipping \
  largeCompositeRegionProbe
```


## Pipette native régionale — 3 novembre 2026

La référence a été relevée avant modification sur 140 prélèvements : pixels
8/16 bits et masques, sept tailles de 1 à 101, bords du document et frontières
de tuiles, alpha variable et source Display P3 normalisée à l’import. Le digest
RGBA64 est identique :
`8cbfba681bbdb74e073f0ded5b4bbc6d854d6aff4d899953dcdb89fd98b20317`.
Pixels stockés et historique restent inchangés. Les tests de valeurs connues,
alpha prémultiplié, masque uniforme et rendu complet final restent présents.

Sonde locale Qt 6.12 : trente échantillons 101 × 101 sur le calque actif d’un
document 24 Mpx/16 bits passent de 86 ms à moins de 1 ms, sans seuil de vitesse
imposé aux tests. Le digest des couleurs reste
`360e0251e3f2961b8964f08ebf2dc0d2f2a1466433a65c80563e69a50e57d746`.
Il s’agit du temps des prélèvements, hors création du document et affichage.

```sh
PHOTOCRAP_EYEDROPPER_PROBE=1 QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  build/photocrap_tests eyedropperNativeTileBoundaryDigest largeNativeEyedropperProbe
```

## Absence de rétention de l’ancien document — 3 novembre 2026

Pour image ordinaire, projet et RAW, une tâche d’ouverture est mise en attente
puis le document affiché est remplacé. Un QImage témoin d’une ancienne tuile
devient son unique propriétaire avant même de libérer le worker : ni la tâche ni
son contexte ne gardent les anciens plans/historique. Le résultat devenu caduc
ne remplace pas le nouveau document. Les sauvegardes, exports et éditions gardent
leurs snapshots requis et continuent de passer leurs comparaisons de pixels et
leurs tests d’annulation. La suite Opérations compte 135 réussites et cinq sondes
optionnelles.

## Préparation complète des RAW sur worker — 3 novembre 2026

Un DNG Bayer original synthétique de 1 024 × 768, orienté à 90°, est développé en
16 bits puis ouvert ou importé. Sur cache disponible ou indisponible, pixels,
calques, masques, outil et taille de brosse correspondent à la référence. Tuilage,
centrage, promotion et déchargement sont déjà terminés avant l’accusé GUI ; l’ancien
document reste affiché jusque-là. Le résultat terminé ne retient aucun tampon
QImage du décodeur. L’import crée une seule étape et Annuler restaure profondeur,
pixels et état enregistré précédents.

Dix scénarios croisent ouverture/import avec annulation en attente ou après
préparation, nouveau document, nouveau geste et destruction. Une promotion
refusée par la limite de stockage logique conserve les douze calques 8 bits du
document 24 Mpx. Le smoke test passe par le dialogue RAW et Échap. Il a aussi révélé que le
raccourci hors du popup modal était bloqué : le popup prend désormais le focus et
son contenu porte le raccourci, vérifié sous Qt 6.8/6.12. La suite
Opérations compte 132 réussites et cinq sondes optionnelles. Les tests RAW
historiques (développement, orientation, erreur et annulation) restent présents.
Ces fixtures originales ne remplacent pas une matrice de boîtiers réels.

## Collecte des groupes du cache sur worker — 3 novembre 2026

Le timer GUI capture les vecteurs de calques partagés sans développer leurs plans.
Un test bloque la file de travail, attend la collecte, puis peint une nouvelle
tuile avant de libérer le worker. Les groupes capturés déchargent les anciennes
versions ; les trois tuiles inchangées sont partagées et la nouvelle tuile de
256 Kio reste en RAM jusqu’à la prochaine collecte. Annuler/Rétablir conserve
les pixels attendus. La suite Opérations compte 117 réussites et cinq sondes
optionnelles. Aucun seuil artificiel de latence n’est imposé aux machines CI.

## Annulation des écritures du cache — 3 novembre 2026

En ARGB32/RGBA64 prémultiplié et Gray8, un arrêt avant écriture ne crée pas de
bloc ; un arrêt après la première tuile mappe seulement celle-ci. Pixels et clés
de versions restent identiques, y compris après nettoyage du cache, puis le
reste peut être déchargé normalement. Le relais PixelPlane est aussi vérifié.

Une file de travail bloquée rend déterministes quatre scénarios : nouveau
document, dossier de cache remplacé, budget RAM relevé et destruction. Le timer
a déjà programmé son déchargement, mais après le changement celui-ci ne décharge
aucun ancien bloc lorsque le worker se libère. Le snapshot témoin garde ses
pixels et son allocation native. La suite Opérations compte 116 réussites et
cinq sondes optionnelles ; la suite Tuiles compte 29 réussites et deux sondes.

## Budget RAM pendant le chargement — 3 novembre 2026

Les projets et images ordinaires sont ouverts avec un budget de 1 Mio, sur disque
disponible ou indisponible. Les plans natifs de 2 Mio sont déjà déchargés avant
l’accusé GUI ; l’ancien document reste affiché jusque-là. Après ouverture, le même
cache expose encore les blocs qui portent les pixels, et les tuiles sur le tas
restent sous le budget dans ces cas. Masques, pixels, calques, profondeur, URL et
état enregistré correspondent au chargement de référence.

Sur disque indisponible, les plans restent en RAM et l’ouverture aboutit. Un
changement du dossier de cache pendant l’attente est conservé ; les anciens leases
restent lisibles. Nettoyer le cache puis reconstruire les images des calques
produit les mêmes pixels. La suite Opérations compte 112 réussites et cinq sondes
optionnelles. Le plan du codec et le JSON sont encore matérialisés temporairement.

## Budget RAM pendant la préparation — 3 novembre 2026

Six cas croisent conversion de profondeur/import avec disque disponible,
indisponible ou limité à 1 Mio. Un budget réglé à 1 Mio est capturé par le worker, au lieu de son
ancien défaut de 256 Mio. Deux plans natifs convertis de 2 Mio chacun sont déjà
sur disque avant l’accusé GUI, alors que le document affiche encore ses pixels et
sa profondeur d’origine. Après application, les plans actifs n’ont plus de tuiles
sur le tas dans ce scénario. L’import prépare également son nouveau plan.

Sur disque indisponible ou saturé par ses blocs encore mappés, la préparation
conserve les pixels restants sur le tas et expose
l’erreur dans l’état du cache, sans échec de l’édition. Dans les deux cas, pixels,
unique étape d’historique, Annuler/Rétablir et nettoyage du cache restent corrects.
La suite Opérations compte 107 réussites, plus cinq sondes optionnelles.
Ce test vérifie le budget des blocs préparés ; anciens plans/historique, buffers
du codec et pages mappées restent distincts du RSS total.

## Conversions natives de profondeur par tuiles — 3 novembre 2026

Les neuf couples ARGB32 prémultiplié/RGBA64 prémultiplié/Gray8 sont comparés au
convertisseur Qt sur des pixels variables, avec métadonnées Display P3, DPR 2 et
résolution. Les mêmes formats gardent leurs clés et leurs leases ; une conversion
change la version logique. Snapshots, mappings et pixels sources restent valides
après conversion, nettoyage du cache ou annulation après une première tuile.
Les conversions non natives gardent le repli QImage.

Sur un document 24 Mpx à six calques et un masque retouché, le passage 8→16 bits
utilise 1 114 144 octets de plans actifs (environ 1,06 Mio) et 524 312 octets
supplémentaires d’historique, sans tampon de canevas complet pour la conversion.
Masques et leurs versions sont inchangés ; Annuler/Rétablir restitue les versions
natives, puis le retour en 8 bits conserve la faible emprise. Ces nombres décrivent
les blocs de plans/historique, pas le RSS total. Les douze cas d’import continuent
à vérifier le centrage, la promotion et l’unique étape d’annulation.

## Placement régional des images importées — 3 novembre 2026

Quarante combinaisons document/source 8/16 bits, dimensions de 1 × 1 à 800 × 600
et DPR 1/2 (`@2x`) sont comparées à un QPainter indépendant sur canevas complet.
Tous les pixels correspondent exactement, après conversion sRGB et format natif.
Annuler/Rétablir conservent le résultat et l’unique étape d’import.

Un PNG de 33 × 27 px dans un document transparent de 24 Mpx/16 bits utilise
524 288 octets de tuiles natives, soit 512 Kio ; les deux plans gardent leur
réservation logique de 384 000 000 octets. Le placement n’alloue plus son tampon
natif à la taille du document. Cette mesure ne représente pas le RSS : décodeur,
composite éventuel et promotion de profondeur sont distincts.

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software build/photocrap_tests \
  regionalLayerImportMatchesIndependentFullCanvasPainting \
  smallImportIntoLargeDocumentAllocatesOnlyItsNativeTiles
```

## Import asynchrone comme calque — 3 novembre 2026

Douze cas combinent document/import 8 ou 16 bits et image plus petite, identique
ou plus grande que le canevas. Les pixels centrés et les calques correspondent
au chemin synchrone ; masques et URL de projet sont conservés. Une réussite
ajoute une seule étape. Annuler restaure aussi la profondeur précédente et l’état
enregistré ; Rétablir restitue l’image et la profondeur importées.

Annulation dans la file ou après décodage, nouveau document/geste, destruction et
fichiers absents/illisibles/non locaux ne publient aucune pile partielle. Le
smoke test passe par la branche Importer du dialogue Ouvrir et vérifie Échap.
La suite Opérations compte 101 réussites et cinq sondes optionnelles. La sonde
`PHOTOCRAP_OPEN_CANCEL_STRESS=1` ajoute l’import PNG de 16 Mpx/16 bits aux
scénarios de projet et d’image simple ; elle est exécutée sous Qt 6.8 et 6.12.

## Décodage d’images en arrière-plan — 3 novembre 2026

PNG 8/16 bits, JPEG, BMP, WebP et TIFF 16 bits, selon les plugins du kit, sont
comparés au chargement synchrone à partir de sources Display P3 avec alpha et
variations natives. Pixels, profondeur, nom et calques correspondent ; le nouveau
document est modifié, son URL de projet est vide et son historique précédent est
effacé. Outil et taille de brosse restent inchangés.

Annulation dans la file ou après décodage, nouveau document/geste, destruction et
fichiers absents/illisibles/non locaux préservent le document courant. Le parcours
du dialogue Ouvrir vérifie Échap et la conservation de l’ancien canevas pendant
l’attente. La suite Opérations compte 81 réussites et quatre sondes optionnelles.
La sonde `PHOTOCRAP_OPEN_CANCEL_STRESS=1` couvre désormais deux jeux de données :
projet `.pcr` et PNG direct, tous deux en 16 Mpx/16 bits. Un appel de codec courant
peut terminer après l’annulation, mais son résultat n’est pas publié.

## Ouverture asynchrone des projets — 3 novembre 2026

En 8/16 bits, les projets contenant pixels, masques, réglages, texte éditable,
tracés et tranches donnent les mêmes pixels et métadonnées que le chargement
synchrone. L’ancien document, son URL, ses pixels et son historique restent
intacts jusqu’à l’accusé GUI ; ensuite le projet chargé est propre, sans historique
précédent, et les réglages d’outil sont conservés.

Annulation dans la file ou après décodage mais avant l’accusé, nouveau document,
nouveau geste et destruction ne publient pas le résultat. Les fichiers invalides
laissent le document courant intact, même si l’erreur survient dans le dernier PNG
ou lors de la validation finale des tracés. Le repli synchrone et l’ouverture d’images
ordinaires conservent leur contrat. Le smoke test passe par le dialogue Ouvrir et
vérifie la progression et Échap. La suite Opérations compte 67 réussites, plus
3 sondes lourdes optionnelles. Une sonde d’ouverture réelle interrompt le décodage
PNG d’un projet 16 Mpx/16 bits et conserve pixels/historique/état modifié ; elle
passe sous Qt 6.8 et 6.12 :

```sh
PHOTOCRAP_OPEN_CANCEL_STRESS=1 QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  build/photocrap_operation_tests runningProjectDecodeCanBeCancelledWithoutReplacingTheDocument
```

## Fonds compacts des plans importés — 3 novembre 2026

Les tests couvrent trois formats natifs (ARGB32 prémultiplié, RGBA64 prémultiplié
et masque Gray8), un premier pixel trompeur, les bords incomplets pondérés par
leurs octets alloués, les bits prémultipliés non canoniques et les métadonnées
Display P3/DPR/résolution. Les remplacements gardent les clés des tuiles inchangées
et les snapshots restent valides après éviction et nettoyage du cache.

Un projet de 24 Mpx/16 bits avec fond blanc, petit trait rouge et masque blanc
retouché est enregistré puis rechargé : les plans utilisent exactement 589 824
octets (576 Kio), contre 216 000 000 octets de réservation logique. Cette mesure
ne représente pas le RSS : décodeur, composite et copies de référence du test sont
séparés. Pixels et masque sont identiques ; une nouvelle retouche du masque,
Annuler/Rétablir et le déchargement complet des tuiles passent sous Qt 6.8/6.12.
Le test de cache des fréquences utilise désormais un détail dans chaque tuile :
les plans uniformes n’ont plus de blocs à écrire. Réutilisation et repli sur disque
indisponible restent ainsi effectivement testés. Le rendu natif Metal passe aussi.

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  build/photocrap_tests sparseLargeProjectReloadPreservesCompactTileFootprints
build/photocrap_tiled_tests
```

## Sauvegarde asynchrone et fermeture différée — 3 novembre 2026

Les sauvegardes de nouveaux projets et de projets déjà associés produisent les
mêmes octets que le helper synchrone en 8/16 bits, avec masques. Avant l’accusé GUI,
le document garde son ancien nom/URL et son état modifié ; après réussite il est
marqué enregistré sans changement d’historique, calques, pixels ou réglages.
Annulation, nouveau geste/document et destruction conservent le fichier précédent.
Les erreurs et le repli synchrone sont couverts, tout comme la finalisation du texte
et un remplacement de document depuis le signal de réussite (URL stable).

Le smoke test simule Enregistrer puis fermer, une fermeture pendant le traitement
et son annulation : la fenêtre reste ouverte et la fermeture différée est oubliée.
Le scénario Quitter/Enregistrer force aussi une vraie tâche asynchrone et attend
sa confirmation avant la fermeture des fenêtres outils.
La suite Opérations compte 57 réussites, avec deux sondes lourdes désactivées par
défaut. La sonde de sauvegarde réelle annule un projet de 16 Mpx/16 bits pendant
l’encodage et garde son ancien fichier et son état modifié :

```sh
PHOTOCRAP_SAVE_CANCEL_STRESS=1 QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  build/photocrap_operation_tests runningSaveCanBeCancelledBeforeFileCommit
```

## Export asynchrone en lecture seule — 3 novembre 2026

Les fichiers correspondent octet pour octet à l’export synchrone pour les formats
PNG, JPEG, BMP et WebP disponibles, en 8/16 bits, avec documents enregistrés ou
modifiés. Historique, calques, pixels, URL du projet et réglages d’outil restent
inchangés. Annulation, nouveau document/geste et destruction pendant la file
conservent le fichier précédent. Les erreurs sont reportées sans état partiel.
Une fin déjà publiée mais pas encore notifiée à la GUI ne peut être présentée
comme annulée ; le bouton est désactivé pendant cette validation finale.
Le parcours du dialogue d’export et Échap sont couverts par le smoke test QML.

La suite Opérations compte 52 réussites sous Qt 6.8/6.12, plus une sonde optionnelle
exécutée localement : un PNG réel de 16 Mpx/16 bits est interrompu pendant
l’encodage, et son fichier précédent demeure intact après la fin du worker.

```sh
PHOTOCRAP_EXPORT_CANCEL_STRESS=1 QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  build/photocrap_operation_tests runningExportCanBeCancelledBeforeFileCommit
```

## Sauvegarde de projet en flux — 3 novembre 2026

Sonde isolée de l’encodeur : douze plans synthétiques opaques de 1 024 × 1 024,
macOS Apple Silicon / Qt 6.12, pic RSS du processus via `/usr/bin/time -l`.
La composition, l’historique et les autres caches de l’application ne font pas
partie de cette sonde. Une exécution par mode donne :

| Profondeur | Ancien pic RSS | Flux, pic RSS | Réduction | Encodage ancien / flux |
|---|---:|---:|---:|---:|
| 8 bits | 324 Mio | 74 Mio | 77 % | 1 927 / 1 703 ms |
| 16 bits | 692 Mio | 122 Mio | 82 % | 5 964 / 5 445 ms |

Les sorties de sonde ont les mêmes tailles et SHA-256 pour chaque profondeur.
Les durées sont des mesures locales ponctuelles. Reproduction sur macOS :

```sh
PHOTOCRAP_PROJECT_ENCODING_PROBE=stream PHOTOCRAP_PROJECT_ENCODING_BITS=16 \
  /usr/bin/time -l build/photocrap_project_write_tests projectEncodingMemoryProbe
# Remplacer stream par legacy pour la référence précédente.
```

Le scénario mémoire est ignoré sans opt-in ; il ajoute un skip volontaire à la
suite dédiée.

La suite dédiée compte neuf réussites : fragments arbitraires autour des multiples
de trois et de 12 Kio, sorties courtes, erreur et écriture nulle, padding et limite
stricte, finalisation répétée et refus après finalisation. Les requêtes Base64 restent
bornées à 16 Kio ; l’échec d’un PNG est propagé et le fichier précédent reste intact.
Les PNG encodés en ARGB32/RGBA64/Grayscale8 donnent exactement les mêmes octets
Base64 que l’ancien tampon QBuffer. Les projets v9 8/16 bits avec masques, calques
compacts, réglages sans pixels, noms multilingues et caractères échappés sont
rechargés à l’identique. Les 301 calques de 24 Mpx et le texte éditable gardent
leurs tests de persistance. Ces contrôles passent sur Qt 6.8 et 6.12. Le codec PNG
peut encore demander des tampons natifs complets ; le passage de la sauvegarde en
arrière-plan est validé dans l’étape suivante.

## Rasterisation régionale du texte — 3 novembre 2026

Les 24 références QPainter pleine image donnent les mêmes pixels : profondeurs
8/16 bits, texte ponctuel/paragraphe, italiques et tailles mélangées, plusieurs
écritures, rotation, inclinaison, échelle, origine négative et sortie du canevas.
Un petit texte sur 24 Mpx reste dans quelques tuiles pendant sa création, son
déplacement et sa copie Alt ; l’annulation restaure les versions initiales.
Revenir à l’ancre restaure exactement les pixels et ne crée pas d’historique.
Les 15 cas de la suite Texte passent sous Qt 6.8/6.12, y compris formats mixtes,
redimensionnement/rotation, projets et pixellisation. Le contrôle de pile compare
les glyphes affichés au composite CPU et vérifie la disparition des anciennes positions.

## Invalidation limitée aux pixels des bordures — 3 novembre 2026

Le test reproduit une clé modifiée par des pixels hors de l’intersection demandée.
Il vérifie ensuite la stabilité après stockage d’une tuile, retour au fond uniforme,
snapshots, vidage du cache disque, mutation transitoire et lectures concurrentes
avec éviction des empreintes. Les pixels ARGB32/RGBA64/Grayscale8 et un changement
d’alpha d’une seule unité native 16 bits sont couverts. Les tuiles entières gardent
leurs versions rapides. Le contrôle de pile limite maintenant à deux transferts
chacun des deux déplacements, contre trois auparavant pour le second.

## Déplacement des calques parcimonieux — 3 novembre 2026

Vingt combinaisons comparent le résultat natif à QPainter pleine image : 8/16 bits,
copie Alt, translation alignée ou fraction de tuile, vers le haut/gauche et hors
canevas. Pixels presque transparents, tuile de bord 1 × 1 et source déchargée dont
le cache a été vidé sont inclus. Les métadonnées P3 et les bits natifs sont conservés ;
les fonds opaques, calques denses et ratios de pixels différents de 1 conservent
le chemin complet. Un petit calque sur 24 Mpx ne matérialise plus le document,
même en copie ou après plusieurs déplacements. Annulation, historique, notifications
de calque/miniature et effacement des anciennes positions sont vérifiés.
Le smoke test de pile contrôle aussi les pixels affichés après deux déplacements
et les transferts limités aux régions touchées, bordures comprises, sur le chemin
matériel. L’optimisation suivante des signatures partielles évite également les
transferts d’un bord dont les pixels restent identiques.

## Réédition des plans d’un pixel — 3 novembre 2026

Le test reproduit le refus de la seconde écriture après stockage tuilé d’un
pixel. Le correctif relit le pixel courant pour l’expansion et pour la clé
régionale de rendu. Écriture identique sans changement de version, réédition,
expansion logique à 600 × 500, snapshots immuables, précision native et métadonnées
Display P3/résolution sont vérifiés en 8/16 bits et niveaux de gris. Un document
1 × 1 est peint deux fois puis annulé/rétabli sans perte ni aperçu périmé.

## Déplacement régional de sélection — 3 novembre 2026

Les douze combinaisons 8/16 bits, déplacement/copie Alt, sélection rectangulaire,
elliptique ou progressive et pixels semi-transparents correspondent exactement
à une référence QPainter sur le calque entier. Un déplacement de 5 000 × 3 000 px
d’une petite sélection sur 24 Mpx/16 bits n’ajoute au plus qu’une tuile native de
512 Kio à la source déjà tuilée, sans tampon pour l’intervalle. Les anciens aperçus
sont effacés du composite, Annuler/Rétablir conserve pixels et sélection, et une
seule miniature est notifiée. Une copie entièrement hors canevas garde son étape
d’historique pour le déplacement de la sélection, même sans pixel changé.

## Aperçus régionaux de formes — 3 novembre 2026

Les 36 combinaisons de six formes, trois contours et deux profondeurs conservent
exactement l’empreinte de pixels de l’ancien rendu, sur une sélection elliptique
progressive traversant les tuiles. Une référence QPainter pleine image vérifie
les étoiles étroites à contour épais et leurs jointures, dans les deux directions.
Le dégradé sélectionné correspond exactement à sa référence QPainter 8/16 bits.
Les tests de 24 Mpx/16 bits bornent une petite forme ou un petit dégradé sélectionné
à une nouvelle tuile de 512 Kio. Rétrécissement, retour à l’ancre, Annuler/Rétablir,
annulation d’un calque créé et notification unique de miniature sont vérifiés.
La marge tient compte du [miter limit Qt](https://doc.qt.io/qt-6/qpen.html#miterLimit)
exprimé en largeurs de plume. Déplacement et dégradés pleine image restent complets.

## Pot de peinture asynchrone — 3 novembre 2026

Les opérations complètes comptent maintenant 42 réussites : comparaisons exactes
synchrone/asynchrone du pot de peinture en 8/16 bits, sur pixels, masque et sélection
progressive ; image et historique inchangés avant la fin, une seule validation et
Annuler/Rétablir. Le pool bloqué vérifie annulation, changement de document et
priorité d’une nouvelle pression Déplacement/Œil rouge/Rectangle avant son premier
glissement. Couleur et opacité sont capturées sans écraser les réglages suivants.
Un remplissage de 24 Mpx est annulé pendant son parcours, après progression réelle,
avant toute écriture ; la source compacte reste intacte. Une couleur déjà identique
ne démarre pas de worker et n’ajoute pas d’historique.

## Pot de peinture parcimonieux — 3 novembre 2026

Empreinte de pixels identique à l’ancien calcul pour seize combinaisons : couleur
8/16 bits, masques, opacité, transparence, sélection progressive et composantes
disjointes. Les composantes non cliquées restent intactes, Annuler/Rétablir conserve
les pixels. Une petite composante de 32 × 32 sur un plan 24 Mpx alloue une seule
tuile de visite de 64 Kio et au plus une nouvelle tuile couleur/masque, après
déchargement de la source. Les vues restent valides après vidage du cache et les
copies QImage modifiables se détachent correctement. Le cas d’un document d’un
pixel déjà édité est inclus. Les suites moteur, Plume et tuiles passent, ainsi
que les contrôles ciblés Qt 6.8. L’invalidation régionale du canevas et une seule
notification de miniature sont vérifiées.

## Remplissage régional de Plume — 3 novembre 2026

Référence QPainter indépendante en pleine image : courbe fractionnaire traversant
les tuiles, sélection elliptique progressive, opacité et couleurs natives 8/16 bits,
puis masques. Sur le cas testé, les masques sont identiques ; un seul pixel couleur
diffère d’un niveau 8 bits ou de 79 unités 16 bits, lors du changement d’origine
d’anticrénelage. Le test borne tout écart à un niveau 8 bits (257 unités 16 bits),
et vérifie pixels extérieurs, précision native et Annuler/Rétablir.
Sur 24 Mpx/16 bits, le petit tracé alloue au plus une tuile couleur de 512 Kio,
puis une tuile de masque de 64 Kio. La suite Plume passe sur Qt 6.8 et 6.12.

## Œil rouge régional — 3 novembre 2026

Les dix-huit combinaisons clic/cadre, taille de pupille et assombrissement
produisent exactement les mêmes pixels avant/après migration, avec transparence,
reflet, sélection progressive et frontières de tuiles. Les pixels extérieurs et
Annuler/Rétablir sont comparés ; les gestes sans effet gardent leur historique.
Sur 24 Mpx/8 bits, une correction réelle ajoute au plus une tuile de 256 Kio,
sans reconstruire le calque complet.

## Écritures identiques et versions de tuiles — 3 novembre 2026

ARGB32, RGBA64 et Grayscale8 : un patch identique traversant quatre tuiles conserve
les identifiants, les versions logiques et zéro allocation RAM après déchargement.
Un seul pixel différent détache exactement une tuile ; les trois autres gardent
leurs identifiants et les anciens snapshots restent lisibles après vidage du cache.
Un plan compact identique reste stocké sur un pixel ; les écritures hors document
ne créent pas de version. Le compteur de modification n’est validé qu’après
application atomique de l’ensemble du patch.

## Bilan des outils à empreintes — 3 novembre 2026

- Build complet Qt 6.12 et 49/49 suites hors contrôles natifs macOS réussies,
  après la migration et les correctifs des plans uniformes.
- Build de l’application et suite moteur Qt 6.8.3 : 249 réussites, aucun échec,
  un test de charge optionnel ignoré. Les contrôles de plans tuilés passent aussi
  sur les deux versions de Qt.
- Canevas et composition de pile contrôlés sur Cocoa/Metal matériel avec les
  nouvelles lectures/écritures de plans ; les deux tests réussissent.
- Les comparaisons avant/après conservent les pixels des modes déjà corrects ;
  la couverture de retouche 16 bits et les régions uniformes d’un pixel ont leurs
  tests de reproduction. Les tests 24 Mpx vérifient les allocations régionales,
  sans prétendre mesurer toute la RAM résidente du processus.

## Correcteurs régionaux — 3 novembre 2026

Les seize combinaisons des deux correcteurs et de leurs huit modes produisent
la même empreinte de pixels qu’avant migration : geste fractionnaire traversant
les tuiles avec sélection progressive. Diffusion, alignement, sources composées,
recherche de donneurs, exclusion des zones déjà traitées, verrouillage et absence
d’historique sans effet restent couverts. Sur 24 Mpx/8 bits, chaque correcteur
modifie réellement les pixels avec au plus une tuile de destination de 256 Kio,
sans couverture pleine image ; Annuler restitue le patch original.

L’ancienne couverture raster du document est retirée du contrôleur commun des
outils à empreintes : ils passent tous par les tuiles ou par leur pointe locale.

## Pinceau mélangeur régional — 3 novembre 2026

Les quatre préréglages reproduisent exactement l’empreinte de pixels précédente
sur un geste fractionnaire traversant les tuiles avec sélection progressive.
Charge, mélange humide/sec, consommation, prélèvements Alt uni/multicolore,
échantillonnage visible, masques et restauration du réservoir à l’annulation sont
testés. Sur 24 Mpx, les deux prélèvements n’ajoutent pas d’allocation du plan ;
le petit geste ajoute au plus une tuile de 256 Kio et aucune couverture complète.

## Forme d’historique artistique par tuiles — 3 novembre 2026

Les dix styles produisent la même empreinte de pixels avant/après migration sur
un geste fractionnaire franchissant les deux axes de tuiles, avec dispersion
déterministe. Tolérance, opacité, sélection, historique et protection restent
couverts. Chaque style est testé pendant un petit geste sur 24 Mpx/8 bits : une
tuile de couverture de 256 Kio et au plus une tuile de destination supplémentaire.
Une zone de 500 px avec pinceau de 1 000 px sur une image de 31 × 29 vérifie les
lectures de source aux bords. Les tests passent sur Qt 6.8 et 6.12.

## Forme d’historique par tuiles — 3 novembre 2026

Empreinte de pixels identique avant/après migration pour trois modes de fusion,
avec source transparente et sélection elliptique progressive franchissant les
frontières de tuiles. Les états futurs, branches supprimées, identité des calques,
effacement par source transparente, opacité/débit et absence d’historique sans
modification restent couverts. Sur 24 Mpx/8 bits après déchargement, le geste
alloue au plus une tuile de 256 Kio de couverture et une tuile de destination ;
vidage du cache, Annuler/Rétablir et pixels exacts restent vérifiés.

## Remplacement de couleur régional — 3 novembre 2026

Les 36 combinaisons de quatre modes, trois échantillonnages et trois limites
produisent la même empreinte de pixels avant/après migration, sur le même Qt.
Le geste traverse les deux axes de tuiles avec une sélection progressive et de
la transparence ; pixels extérieurs, Annuler/Rétablir et annulation sont vérifiés.
Le scénario 24 Mpx/8 bits vérifie pendant le geste au plus une tuile de 256 Kio
de couverture et une tuile supplémentaire pour le plan, puis l’historique.

## Doigt régional — 3 novembre 2026

Les empreintes des pixels avant/après migration sont identiques pour le plan
couleur 8 bits et le masque. Ellipse de sélection progressive traversant les
frontières de tuiles, angle/aplatissement, reprise des couleurs, peinture au doigt,
Alt, échantillonnage visible, Annuler/Rétablir et annulation restent testés.
Le masque d’un document 16 bits utilise aussi ce chemin. Sur 24 Mpx/8 bits, la
destination du petit geste ajoute au plus une tuile de 256 Kio, sans couverture
pleine image. Le Doigt couleur 16 bits reste explicitement non pris en charge.

## Métadonnées des plans compacts — 3 novembre 2026

Expansion régionale d’un plan d’un pixel en ARGB32, RGBA64 et Grayscale8, avec
profil absent ou Display P3, ratio 2 et résolutions X/Y différentes : couleurs
natives, métadonnées et snapshot initial conservés, une seule tuile allouée.
Les sources nulles ou non uniformes sont refusées par le constructeur dédié.
La lecture avant écriture est aussi comparée pour une région d’un pixel aux
coordonnées (10, 10) et une région de 7 × 9 : valeur uniforme, profil, ratio et
résolutions préservés dans les trois formats. Elle ne dépend pas de la position
du pixel stocké à l’origine.

## Tampons par tuiles — 3 novembre 2026

La duplication est comparée pixel par pixel à un QPainter travaillant sur les
images complètes en 8/16 bits : source transparente, décalages fractionnaires,
frontières de tuiles et bords du document. Les tests existants couvrent alignement,
sélection, sources dessous/tous calques, masques, motifs personnalisés et historique.
Sur 24 Mpx/16 bits, les tampons de duplication et de motif sont contrôlés pendant
un geste sur pixels puis masque : au plus une tuile de couverture de 512 Kio et
une tuile supplémentaire de pixels ; annulation et libération de la couverture.

## Retouches locales par tuiles — 3 novembre 2026

Flou, Netteté, Densité −/+, Éponge : comparaison avant/après migration des empreintes
de tous les pixels 8 bits, identiques sur le même kit Qt. En 16 bits, le nouveau
test reproduit d’abord un geste sans effet : l’ancienne lecture de couverture
utilisait un pointeur 8 bits. Le correctif est vérifié par une modification réelle,
profondeur native préservée, sélection progressive franchissant les deux axes de
tuiles, pixels extérieurs identiques, Annuler/Rétablir et annulation du geste.
Un document 24 Mpx/16 bits vérifie pendant le geste le plafond d’une tuile de
512 Kio pour la couverture et d’une tuile supplémentaire pour le plan modifié.

## Libération du résultat RAW — 3 novembre 2026

Le test DNG asynchrone vérifie que `retainedDecoderBytes` vaut zéro après ouverture,
import comme calque, annulation et fichier absent. Le document reste intact en cas
d’erreur ou d’annulation, et l’import reste annulable. Ce compteur concerne le
résultat terminé du worker, pas sa mémoire de travail pendant le décodage.
Le scénario passe avec Qt 6.8.3 et 6.12.0 ; les suites `document` et
`document_operations` repassent après ce changement.

## Gestes récents et résultats périmés — 3 novembre 2026

Pool bloqué : commencer un réglage, un survol de fusion, un tracé ou une tranche
annule l’opération complète avant la validation du geste. Après reprise du worker,
dimensions, paramètres, tracés, tranches et historique restent ceux du geste récent.
Une édition de texte en cours est finalisée avant délégation d’une transformation ;
Annuler restaure ensuite la géométrie précédente en conservant le texte. Les 33 cas
de la suite des opérations asynchrones passent.

Le contrôle Cocoa/Metal utilise les événements Qt de QTest pour Annuler et Échap.
Si macOS refuse de donner le premier plan au processus lancé depuis le terminal,
le test établit son contexte de focus Qt ; il n’injecte aucune touche système.


## Réduction rapide régionale CPU — 3 novembre 2026

Comparaison exacte à Qt FastTransformation sur plans couleur 8/16 bits et masques
8 bits : dimensions impaires, lignes/colonnes, côté de 16 383 px, trois modes
d’aspect, ratios de pixels et espaces couleur. Les plans source restent mappés
sans buffer natif retenu. Les tests passent sur Qt 6.8 et 6.12. Les agrandissements
utilisent la référence Qt ; les interpolations lisses conservent leur chemin actuel.

Mesure isolée de cinq réductions 6000 × 4000/16 bits vers 1000 × 667, source en
RAM : environ 41 ms par pixels régionaux, 102 ms par reconstruction native et
réduction Qt. Les sorties sont exactement identiques ; cela ne mesure pas tout
le rendu, les accès à un disque froid ni la latence d’interaction de l’application.


## Lecture de projets par plans scellés — 3 novembre 2026

Les PNG décodés sont convertis en tuiles immédiatement, avant le plan suivant.
La validation complète du projet précède toujours son application au document.
La suite moteur couvre les formats historiques, les projets corrompus et la
fidélité 8/16 bits ; le scénario dense conserve aussi Annuler/Rétablir et les pixels
après rechargement.

Après les trois optimisations d’allocation (conversions sans effet, import direct,
lecture de projets), le scénario dense rejoué sans compilation concurrente passe
de 3 359 752 192 à 2 647 834 624 octets de RSS maximal, environ −21 %, et de
2 562 313 216 à 1 831 045 592 octets d’empreinte maximale. Les durées sont environ
14 et 13 s, sans prétention à un benchmark de vitesse. Ces mesures incluent les
deux documents coexistants et varient avec la résidence des mappings et le système.


## Import à la taille du document — 3 novembre 2026

L’import à ratio 1 et dimensions identiques évite le buffer vide et sa peinture.
Chaque pixel d’une source transparente 8/16 bits est comparé au PNG décodé natif,
puis une modification est annulée. Les imports à centrer gardent leur composition.


## Conversions de plan sans changement — 3 novembre 2026

Un plan 16 bits déchargé sur disque conserve zéro allocation RAM et la même
version lorsque le format et l’espace couleur demandés sont déjà actifs. Une
conversion effective vers 8 bits est comparée à Qt et préserve le snapshot natif.
L’import cesse ainsi de reconstruire les calques existants à profondeur identique.


## Documents denses et volume saturé — 3 novembre 2026

- Quatre calques remplis de 24 Mpx/16 bits : 768 Mo de plans, 1 536 blocs disque,
  allocations RAM des tuiles ramenées à zéro. Un trait alloue au plus 1 Mio ;
  versions, Annuler/Rétablir et sauvegarde/rechargement conservent les pixels natifs.
- Séquence complète avec les deux documents coexistants : environ 14 s, pic RSS
  3 359 752 192 octets, empreinte maximale 2 562 313 216 octets sur ce Mac.
  Les buffers temporaires PNG/projet/import et les pages mappées sont inclus.
- Volume HFS+ de 32 Mio effectivement rempli : refus du cache, maintien de la
  nouvelle tuile en RAM, anciennes versions mappées intactes, puis édition après
  vidage. Le script de test démonte et supprime son volume temporaire.


## Niveaux réduits GPU — 3 novembre 2026

- Chaque pixel des niveaux 1/2, 1/4, 1/8 et 1/16 d’une image 1057 × 545 est
  comparé à la réduction Qt du composite natif, en 8/16 bits, avec un calque
  transparent, opacité et masque. Écart maximal observé : une unité d’affichage
  sur Metal Qt 6.8/6.12 et OpenGL moderne Qt 6.12, raccords compris.
- Les allers-retours entre niveaux déjà calculés n’ajoutent ni transferts,
  recompositions natives, réductions GPU ou réduction CPU. Les textures suivies
  de ce canevas atteignent environ 21 Mio en 8 bits et 49 Mio en 16 bits ; ces
  chiffres ne mesurent ni la RAM totale du processus ni la VRAM privée du pilote.
- Sur ce Mac, dix zooms déjà en cache donnent une soumission de trame médiane
  d’environ 16,6 ms (cadence 60 Hz) sur Metal et OpenGL. C’est un délai jusqu’au
  signal `frameSwapped`, pas la durée d’un kernel GPU ni une comparaison avant/après.
- Petit budget : repli CPU complet et retour GPU. Libération/recréation du
  graphe : compteurs à zéro, puis restitution correcte. Le backend OpenGL basic
  pouvant ignorer la demande de libération anticipée, le test recrée sa fenêtre
  dédiée pour forcer la destruction des nœuds et ressources.
- Les 49 tests hors interaction native macOS passent avec Qt 6.12. Moteur,
  opérations, budget, niveaux réduits, pile et filtres passent aussi en logiciel
  avec Qt 6.8. Les 64 cas spatiaux natifs Metal restent verts. Les backends
  Vulkan/Direct3D et une perte réelle de périphérique ne sont pas validés ici.


## Budget graphique partagé — 3 novembre 2026

- Tests de partage de réserve entre clients, cibles différentes, ressources
  visibles dépassant la cible, désactivation, retrait et comptage par catégorie.
  Les notifications peuvent relire le budget sans verrou réentrant.
- Rendu du canevas, cache LRU, pile, filtres spatiaux et teintes passent en
  logiciel ; le canevas et les 64 cas spatiaux passent sur Metal. Le compteur
  total correspond à la somme des catégories et conserve son pic.
- Les tailles des textures demandées sont comptées ; les allocations privées
  Qt/pilote et les ressources temporairement en vol ne sont pas un plafond garanti.


## Opérations complètes asynchrones — 3 novembre 2026

- 24 comparaisons exactes synchrone/worker en 8/16 bits : redimensionnement,
  recadrage, profondeur, remplissage avec transparence, effacement sélectionné,
  fusion, inversion du masque, flou et les quatre rotations/miroirs. Une seule
  notification d’historique, puis Annuler/Rétablir, pixels/masques et dimensions.
- Pool bloqué : Annuler et Annuler/Rétablir rendent la main sans attendre ;
  sélection de calque, nouveau document et destruction rejettent les anciennes
  demandes. Une demande de taille invalide préserve le document.
- Les 47 tests hors interaction native macOS passent, après adaptation de trois
  tests historiques pour attendre leurs gros remplissages. Les tests dédiés
  passent aussi sur Qt 6.8. Le contrôle natif Metal vérifie le panneau de
  progression, le bouton Annuler et Échap avec activation AppKit de la fenêtre.
- La progression des routines Qt monolithiques est indéterminée ; l’annulation
  rejette leur résultat sans promettre de les interrompre au milieu d’un appel.


## Filtres spatiaux GPU — 3 novembre 2026

- 64 configurations comparées au composite natif : boîte, gaussien, directionnel,
  netteté et passe-haut, rayons/distances 0, 1, 8, 31 et 100, en 8/16 bits. Les
  points de contrôle couvrent les limites du document, les raccords horizontaux
  et verticaux, les masques/opacités, les chaînes de deux filtres et l'alpha.
- Sur Metal Qt 6.8 et 6.12, et OpenGL moderne Qt 6.12, l'écart maximal observé aux
  points de contrôle est d'une unité d'affichage 8 bits. Cette mesure ne constitue
  pas une comparaison exhaustive de tous les pixels ni un gain de vitesse global.
- Un test bloque le pool CPU : le changement GPU reste visible avant toute
  notification d'image CPU. Annuler retrouve l'image précédente et n'ajoute pas
  d'historique. Les paramètres périmés ne sont pas réappliqués par le worker.
- Le passage 1056 × 96 → 544 × 544 → 1056 × 96 vérifie la réutilisation d'une
  tuile précédemment hors écran. Le retrait du document ramène à zéro les
  allocations suivies de la pile et des cibles spatiales.
- Les 45 tests hors interaction native macOS passent sur Qt 6.12. Les tests
  moteur, pile et filtres passent aussi sur Qt 6.8 en logiciel ; les aperçus
  tonals existants restent verts. L'ancien contexte OpenGL sur Mac utilise
  bien le repli CPU, sans erreur de shader. Le test OpenGL moderne est lancé avec
  `PHOTOCRAP_TEST_GL_CORE=1 QSG_RHI_BACKEND=opengl QSG_RENDER_LOOP=basic` ; la variable
  de profil n'est prise en compte que par les builds de test.


## Première composition GPU de la pile — 3 novembre 2026

- Comparaisons Metal en 8/16 bits pour les 21 modes autorisés, opacité partielle,
  masque gris, limites de tuiles, écrêtage sur une base masquée et masquage de la
  base. Tolérance de deux unités d'affichage 8 bits ; le composite natif CPU
  reste inchangé et fait référence pour les fichiers et les exports.
- Une image inchangée n'exécute aucune nouvelle passe et ne retransfère aucune
  entrée. Une touche traversant une limite de tuile transfère uniquement les
  deux régions touchées. L'ajout d'un filtre non pris en charge repasse au CPU
  et libère les cibles de composition sans crash.
- Les tests de stockage vérifient que les identifiants de région restent stables
  après déchargement disque et modification distante, et changent pour une
  retouche locale. Le budget des tuiles visibles est estimé avant allocation.
- `stack_render`, le moteur, les tuiles, le canevas logiciel et le smoke QML
  passent sur Qt 6.12 ; le moteur, les tuiles et `stack_render` passent aussi sur
  Qt 6.8, y compris sur Metal. Les autres backends matériels restent à vérifier ; aucun
  gain de latence global n'est déduit du seul nombre de transferts évités.

## Plans éditables tuilés — 3 novembre 2026

- Les calques, masques et sources partagent des versions immuables de tuiles.
  Les tests vérifient les écritures régionales exactes en 8/16 bits, les frontières
  des tuiles et la conservation des anciennes versions après modification.
- Le cycle édition, déchargement disque, nettoyage du cache, Annuler/Rétablir,
  sauvegarde PCR et réouverture conserve les pixels. Un refus de stockage garde
  les données éditables en RAM ; ce test ne simule pas un volume réellement plein.
- Sur un document transparent 6 000 × 4 000 en 16 bits, une petite touche de
  pinceau puis de masque n'alloue que 576 Kio pour leurs plans. Le lot de trois
  tests de stockage atteint 402 Mio de RSS maximum et 342 Mio de mémoire
  « footprint » sur ce Mac (`/usr/bin/time -l`, Qt 6.12). Ces chiffres incluent
  les composites et buffers temporaires ; ce ne sont pas des plafonds globaux.
- Les tests document, tuiles et aperçus GPU passent sur Qt 6.8 et 6.12. Le test
  du masque peint vérifie son identité logique après reconstruction depuis les
  tuiles, sans dépendre de la clé d'un QImage temporaire.

## Masques sans contribution — 2 novembre 2026

- Tests 8/16 bits d’un réglage Mosaïque avec masque compact noir : retouche
  régionale, désactivation/réactivation du masque, densité 0/100 et calque
  écrêté sur cette base masquée. Le retour à un masque visible réactive l’effet.
- La densité nulle rend directement l’image source ; la comparaison scalaire
  des compositions avec masques vérifie aussi ce cas.

## Calques sans contribution visible — 2 novembre 2026

- Tests 8/16 bits avec les 27 modes de fusion à opacité nulle, réglage Mosaïque
  désactivé par son opacité, retour à une opacité visible, et écrêtage sur une
  base masquée ou d’opacité nulle.
- Une retouche locale sous ces calques conserve une région de mise à jour
  plus petite que le document, au lieu de forcer la recomposition intégrale.

## Multiplication des masques 8 bits — 2 novembre 2026

- Comparaison exhaustive des 65 536 combinaisons canal/couverture au calcul
  flottant arrondi ; composition vérifiée avec alpha et densités intermédiaires,
  en séquentiel et parallèle. Le chemin 16 bits conserve son calcul propre.
- Sur une image synthétique de 12 Mpx avec huit threads, application d’un
  masque 8 bits et copie passent d’environ 25 à 10 ms. Cette mesure isolée
  ne comprend pas l’ensemble de la pile ni l’affichage.

## Extraction directe de l’alpha — 2 novembre 2026

- Comparaison aux conversions Qt sur les 65 536 valeurs d’alpha, huit formats
  QImage et sept opacités, dont les valeurs hors plage ; source non modifiée.
- Sur une image synthétique 16 bits de 12 Mpx, cinq extractions successives
  passent d’environ 109 à 92 ms. La conversion couleur temporaire de 48 Mo
  est supprimée ; ces chiffres ne mesurent pas toute la composition.

## Couverture des calques écrêtés — 2 novembre 2026

- Les tests en 8 et 16 bits couvrent plusieurs calques écrêtés, les changements
  d’opacité et de visibilité de la base, et le passage à une autre base.
- Mesure synthétique isolée sur cinq calques écrêtés au-dessus d’une base 16 bits
  de 12 Mpx : préparation de la couverture environ 110 → 19 ms. La mesure ne
  comprend pas le masquage des calques, leur fusion ni l’affichage.

## Application des masques de calque — 2 novembre 2026

- Comparaison exacte à une référence scalaire sur des images 8 et 16 bits,
  avec transparence, densités 0/63/100 et surfaces de part et d’autre du seuil
  de parallélisation. Le même projet est aussi composé depuis le pool de calcul.
- Sur une image synthétique 16 bits de 12 Mpx avec huit threads, application
  du masque et copie de l’image passent d’environ 114 à 42 ms. Ce chiffre ne
  comprend ni l’ensemble de la pile de calques ni l’affichage GPU.

## Flou de boîte 16 bits — 2 novembre 2026

- Comparaison exacte à une référence scalaire pour trois tailles, dont une
  ligne et une colonne, et les rayons 1, 7 et 30. Les tests couvrent aussi les
  bords et l’alpha prémultiplié ; le test de composition régionale reste valide.
- Sur une image synthétique de 12 Mpx avec huit threads, la passe verticale
  mesurée isolément passe d’environ 193 à 61 ms et produit les mêmes pixels.
  Ce chiffre n’inclut ni la passe horizontale ni l’affichage ou les autres effets.

## Animation des contours hors de la vue — 2 novembre 2026

- Tests Metal sur Qt 6.8 et 6.12 : rectangles et ellipses en 8 et 16 bits,
  aucun travail de préparation de phase ni transfert de géométrie quand aucun
  segment n’est visible, puis reprise avec la phase exacte au retour du bord.
- Le repli logiciel conserve son fonctionnement ; l’horloge de phase continue
  de tourner afin de conserver la synchronisation avec les autres guides.

## Surimpressions hors de la vue — 2 novembre 2026

- Les tests couvrent les régions fragmentées, les contacts aux bords, les UV de
  Pièce et les limites du maillage. Une comparaison locale avec les anciens
  chemins sur 300 régions, échelles et vues conserve les mêmes sommets.
- Sur une région synthétique de 4 000 rectangles hors écran, 1 000 préparations
  successives des deux surimpressions passent d’environ 25 ms à 0,01 ms sur ce
  Mac. Il s’agit de la préparation CPU, pas du temps de rendu GPU complet.

## Contours de sélection hors de la vue — 2 novembre 2026

- Les tests vérifient les traits sur la limite de découpage, les courbes dont
  seuls les points de contrôle entrent dans la vue et les contours complexes
  extérieurs. Une comparaison avec l’ancienne implémentation couvre 400
  contours et vues variés, sans changement de sommets.
- Mesure locale synthétique pour 200 préparations répétées d’un contour de
  12 000 segments entièrement hors de la vue : environ 42 ms → 0,02 ms. Le
  rectangle englobant est alors déjà en cache dans le chemin Qt ; ce résultat
  ne mesure pas le rendu GPU complet ni une sélection nouvellement construite.

## Préparation des contours de sélection — 2 novembre 2026

- Les raccords à angle droit, demi-tours, chemins fermés, zones découpées et
  limites du maillage sont vérifiés. Une comparaison locale avec l’ancienne
  implémentation donne les mêmes sommets sur 400 contours et vues variés.
- Sur un contour synthétique de 12 000 segments visibles, la préparation CPU
  mesurée localement passe d’environ 227 à 206 ms pour 200 calculs. Ce résultat
  ne mesure pas le temps de rendu GPU complet.

## Sélection des textures en cache — 2 novembre 2026

- Comparaison avec l’algorithme précédent sur 500 ensembles déterministes de
  80 tuiles, budgets variés, égalités et tailles irrégulières. Les tuiles
  visibles restent conservées même si elles dépassent le budget.
- Mesure locale synthétique sur 8 192 entrées et 200 appels : environ 75 → 1 ms
  avec budget nul et 101 → 48 ms quand toutes les tuiles tiennent. Un budget
  intermédiaire nécessitant le tri donne environ 66 → 69 ms. Ces mesures
  concernent la sélection CPU du cache, pas le temps de rendu GPU complet.

## Animation de sélection hors écran — 2 novembre 2026

- Le test d’interface vérifie l’arrêt de l’animation lorsque la fenêtre est
  masquée ou réduite, puis sa reprise dans les deux cas après réaffichage.
- La sélection elle-même reste inchangée ; cette étape évite seulement les
  mises à jour périodiques de la phase quand la fenêtre n’est pas visible.

## Accueil et miniatures des fichiers récents — 2 novembre 2026

- Builds et `qml_smoke` sur Qt 6.8.3 et Qt 6.12.0 ; le test couvre les
  miniatures PNG et JPEG, les chemins accentués et contenant `%`, l’aperçu RAW
  intégré, la reconstruction d’un projet `.pcr`, le remplacement quand un fichier
  est illisible et la poignée de redimensionnement.
- Un DNG synthétique avec aperçu JPEG teste aussi l’extraction LibRaw dans le
  moteur ; le DNG sans aperçu vérifie le retour vide. Le fichier réel
  `IMG_9908.CR2` a permis de vérifier l’orientation et le décodage réduit.
- Le chargement des vignettes est asynchrone. Les projets `.pcr` sont composités
  puis mis en cache ; les RAW dépourvus d’aperçu intégré affichent un emplacement
  de remplacement.

## Préparation GUI des phases de sélection — 2 novembre 2026

- Builds Qt 6.8.3 / 6.12.0 et tests `canvas_render`, `tone_render`, `qml_smoke` ;
  `tile_cache` passe aussi sur le kit local. Formatage et clang-tidy propres.
- Contrôles natifs Metal (Qt 6.8.3 et 6.12.0) et OpenGL (Qt 6.12.0) sur macOS
  Apple Silicon : animation visible sans nouvelle préparation complète des
  snapshots/tuiles, géométrie et textures conservées, phases fractionnaires,
  rectangles/ellipses et lassos en 8/16 bits.
- Une baisse du budget mémoire coalescée avec une phase doit réellement évincer
  les tuiles hors champ. Une modification de visibilité de calque coalescée avec
  une phase doit rafraîchir la photo, puis retrouver ses pixels après restauration.
  Ces contrôles vérifient que le chemin court ne masque pas les demandes complètes.
- La synchronisation du thread graphique reste inchangée. Aucun gain chiffré en
  millisecondes ou en fréquence d’affichage n’est déduit des seuls compteurs.
- Vulkan/Direct3D restent à contrôler sur leurs plateformes.

## Candidats de lasso sans bord visible — 2 novembre 2026

- Builds Qt 6.8.3 / 6.12.0 et contrôles `lasso_guide`, `qml_smoke` ;
  `selection_mesh` passe aussi sur le kit local. Clang-tidy et formatage propres.
- Comparaisons natives Metal et OpenGL sur macOS Apple Silicon, en 8/16 bits.
  À 8×, le contour entoure la vue avec tous ses segments hors champ :
  `layer.enabled` reste désactivé, aucun Canvas n’est chargé sur le chemin GPU
  et les changements de phase ne rappellent pas `updatePaintNode`.
- Les compteurs de géométrie, transferts et paramètres restent stables dans ce
  cas. Revenir sur un bord conserve le même composant et la dernière phase,
  réactive sa composition et retrouve les pixels de la référence indépendante.
- Le repli logiciel garde son rendu. Les contrôles matériels Vulkan/Direct3D
  restent à exécuter sur leurs plateformes.

## Pointillés des candidats combinés du lasso — 2 novembre 2026

- Compilation et tests `lasso_guide` / `qml_smoke` sur Qt 6.8.3 et Qt 6.12.0 ;
  `selection_mesh` passe aussi sur le kit local. Contrôle clang-tidy des deux
  nouveaux fichiers C++ sans avertissement.
- Comparaisons natives sur macOS Apple Silicon : Metal 1×/1,25×/2× et OpenGL, avec
  référence Canvas indépendante en 8/16 bits. Phases fractionnaires et négatives,
  extrémités plates, croisements, zoom 64×, défilement, rotation et faible opacité.
- Les compteurs attestent que la phase conserve le maillage et ses buffers,
  sans repeindre de Canvas. Entrées non finies/hors budget, angles aigus,
  raccords sous-pixel au zoom 0,005×, transition Alt, masquage et réaffichage
  conservent le repli et les pixels. Les surfaces aux dimensions fractionnaires
  sont comparées avec la même tolérance de filtrage que les surfaces tournées.
- Les angles presque superposés du guide uni ont un repli plus conservateur
  (cosinus supérieur à 0,99), pour préserver les pointes QPainter aussi à 1×.
  Sur écran fractionnaire, le guide uni garde le Canvas à l’origine et à la taille
  de la vue pour éviter un changement de filtrage ; les nouveaux pointillés restent GPU.
- Vulkan et Direct3D n’ont pas été exécutés localement.

## Guides de tranches après v0.16 — 2 novembre 2026

- Build local et contrôles `slice_cache`, `slice_overlay`, `qml_smoke` sur Qt 6.8.3
  et Qt 6.12.0. Comparaisons natives sur Metal 1×/2×, OpenGL et repli à échelle
  d’écran fractionnaire, sur macOS Apple Silicon.
- Référence raster indépendante sur documents 8/16 bits, tranches automatiques,
  petites étiquettes débordantes, zoom jusqu’à 64×, rotation, superposition de
  poignées et opacité de l’ensemble des guides.
- Identité du modèle et des objets conservée au petit défilement ; absence de
  repeint du grand Canvas et du cache de chiffres observé. Repli au-delà de 128
  descriptions visibles et destruction des surfaces au masquage/retour GPU.
- Les vérifications de rendu matériel Vulkan/Direct3D restent à exécuter sur les
  plateformes concernées ; ces résultats locaux ne les remplacent pas.

## Validation de v0.16 — 2 novembre 2026

- Les 43 tests hors écran passent localement sur Apple Silicon avec Qt 6.8.3 et
  Qt 6.12.0. Les builds, tests et paquets des trois plateformes sont validés dans le
  [workflow de release](https://github.com/fguilleme/PhotoCrap/actions/runs/41430300211).
- Le test de survol des presets force le placement de la ListView avant de déplacer
  la souris : sous Qt 6.8, l’ouverture précède l’activation des lignes pour les événements.
  Les tests de survol, annulation, clic, clavier et historique restent actifs.
- Sous Windows, CTest fournit le dossier système des polices à Qt offscreen lorsque
  `QT_QPA_FONTDIR` n’est pas déjà défini. Le test Texte exige une vraie base de polices.
- Les suites GPU paramétrées disposent de 90 secondes au total sur les runners ;
  leurs assertions et délais individuels restent inchangés.
- Un worker volontairement retenu reproduit la concurrence entre le contour
  progressif et le proxy auxiliaire : la dernière demande de masque doit rester
  prioritaire. Le test couvre 8/16 bits, le rendu auxiliaire et Annuler/Rétablir.
- Le test de survol de la plume attend le rendu initial puis celui du guide
  provisoire avant de vérifier l’absence de repeint du tracé et des poignées.
- La CI utilise `macos-15` après le retrait de `macos-14`. En cas d’échec, les
  artefacts `Test-diagnostics-*` conservent les rapports CTest et Texte. Les messages
  Qt Windows sont redirigés vers stderr dans le workflow.

## Validation du 30 septembre 2026

Environnement exécuté : macOS Apple Silicon, AppleClang 21, Qt 6.11.2, CMake 4.4.3, compilation Release.

| Vérification | Résultat |
|---|---|
| Configuration et compilation CMake dans `~/Dev/PhotoCrap` | Réussies |
| Tests du moteur, 11 scénarios Qt Test | Réussis |
| Démarrage QML et rendu hors écran | Réussis, aucune erreur QML |
| Dessin via événements souris, annulation, saisie et protection des modifications | Réussis |
| Installation CMake et déploiement des dépendances Qt | Réussis |
| Démarrage du bundle `dist/PhotoCrap.app` avec Cocoa | Réussi |
| Tests d’interaction et rendu dans le bundle Cocoa | Réussis |
| Chargement depuis Homebrew ou `/usr/local` pendant ce test du bundle | Aucun, vérifié avec `DYLD_PRINT_LIBRARIES=1` |
| Liens symboliques du bundle vers l’extérieur ou liens cassés | Aucun |
| Compilation Linux / Windows | Non exécutée localement ; workflow GitHub fourni |

Le test CTest complet contient deux cibles, `document` et `qml_smoke` : 2/2 réussies.

Le bundle macOS embarque le plugin Cocoa. Le test hors écran documenté dans le README utilise le **bundle de développement** et le Qt installé, qui fournit aussi le plugin offscreen. Pour vérifier le bundle installé, utiliser son interface native :

```sh
./dist/PhotoCrap.app/Contents/MacOS/PhotoCrap --smoke-test
```

Le bundle compilé ici est destiné à macOS arm64. Il n’est pas signé avec une identité Developer ID et n’est pas notarisé. Les binaires d’autres plateformes doivent être produits avec leurs kits Qt respectifs.
