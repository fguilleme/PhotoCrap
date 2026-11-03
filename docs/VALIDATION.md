# Validation

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
