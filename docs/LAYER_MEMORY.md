# Calques, mémoire et cache de travail disque

## Retouches natives 8/16 bits

Les deux correcteurs, Suppression, Pièce, Déplacement basé sur le contenu et
Œil rouge lisent et écrivent désormais les canaux prémultipliés natifs 8/16 bits.
Les recherches et champs de diffusion gardent leurs coordonnées couleur usuelles
0–255, avec les fractions des canaux 16 bits ; la quantification ne se fait qu’à
l’écriture native. La texture du donneur, son alpha et le mélange de couverture
ne passent pas par une image QRgb en 16 bits. Les pointes et sélections restent des
couvertures 8 bits. Les empreintes, recherches, reconstructions et écritures gardent
leurs régions de travail antérieures. Les outils à réparation asynchrone conservent
leurs snapshots, annulation et publication atomique ; certaines captures et copies
pour l’historique peuvent toujours matérialiser un plan entier.

## Migration du 3 novembre 2026

`PixelPlane` stocke désormais les plans de calques, masques et sources de fréquences
et leurs versions d’historique dans `TiledImage`. Les tuiles inchangées sont partagées ;
les outils à empreintes créent uniquement les versions touchées : Pinceau, Crayon,
Gomme, Flou, Netteté, Densité −/+, Éponge, tampons, Doigt, Remplacement de couleur,
Pinceau mélangeur, formes d’historique et correcteurs. Le composite CPU lit les
régions nécessaires sans retenir de copie complète du calque. Les autres outils,
transformations, filtres et sauvegardes peuvent encore matérialiser une image
complète temporaire. Les profondeurs prises en charge par chaque outil ne changent pas.

La création d’un document blanc ou transparent construit aussi directement son
fond natif logique, sans QImage provisoire de la taille du document ni parcours de
scellement de ce fond. Le pixel stocké est en sRGB et dans la profondeur demandée ;
les dimensions et l’identité du plan sont celles du document. La première retouche
ne crée que ses tuiles. Cela ne change pas la réservation logique du calque, les
limites de dimensions ni les tampons éventuels des filtres, affichages et exports.

La création d’une séparation de fréquences sur un calque de pixels à la taille du
document, sans masque actif ou avec un masque neutre, partage directement ses
versions de tuiles entre la source et les deux bandes. Un masque blanc uniforme,
compact ou tuilé sans pixels explicites, ainsi qu’une densité nulle sont reconnus
avant toute expansion de la source. Le contour progressif d’un blanc uniforme
reste blanc et la densité nulle ignore le masque, comme dans le calcul précédent. Modifier ensuite le calque d’origine ne change pas
cette source figée. Un fond compact sans masque actif ou avec un masque neutre devient un `PixelPlane`
de la taille logique du document, avec son seul pixel de fond et sans tuile explicite.
La normalisation de ce pixel reprend l’arrondi QColor de l’ancienne expansion.
Les masques qui modifient les pixels et les sources composées demandent encore
une préparation complète. Pour les grandes sources, elle passe par le worker
commun annulable : snapshot immuable, masque/composition, scellement des tuiles
et budget RAM/disque avant publication. Un composite natif déjà terminé est
partagé avec le worker au lieu de recalculer la pile. Les sources natives ou
compactes neutres gardent leur voie immédiate. Une fin valide crée les deux
bandes dans une seule étape d’historique et ouvre les propriétés ; une annulation
ou un état périmé ne publie rien. Les identités des paires évitent aussi les liens
existants, dynamiques ou préparés, après réouverture. Le zoom ne change pas quand
les dimensions restent identiques. La précision et les calculs des bandes restent identiques. Le partage réduit les blocs physiques supplémentaires,
sans changer les limites de réservation logique ni le format du projet.

`Layer` est défini dans `src/Layer.h` ; les règles de couverture et de visibilité
sont communes dans `src/render/LayerVisibility.h`. La pile GPU utilise le même
point de départ opaque que le CPU et élimine les mêmes plans sans couverture.
Les masques blancs ou de densité nulle n’y créent plus d’entrée de masque, même
avec contour progressif. Un effet incompatible entièrement caché sous un aplat
opaque ne force plus le CPU ; dès que cette couverture cesse, le repli complet
revient. Les contraintes d’effets, formats et modes de fusion réellement visibles
restent applicables. Le suivi graphique ne compte que les entrées conservées ;
il ne mesure toujours pas la VRAM physique du pilote.

Le composite CPU commence désormais au dernier calque de pixels uniforme qui
recouvre entièrement le document en mode Normal, à opacité et alpha exactement
pleins, sans écrêtage ni masque modifiant la couverture et dans un groupe visible.
Les couches inférieures ne sont ni rendues ni prises en compte pour les marges et
les restrictions de lecture régionale. Les pixels restent présents et éditables ;
seul ce rendu les ignore. Opacité fractionnaire, alpha 65 534/65 535, mode de fusion,
masque gris/noir, groupe caché, image non uniforme ou DPR différent de 1 empêchent
ce raccourci. Les couches écrêtées suivantes utilisent toujours l’aplat comme base.

Le composite CPU consulte aussi les métadonnées des plans uniformes avant de
matérialiser leurs pixels. Un fond natif transparent ne fournit aucune couverture ;
un masque noir uniforme à pleine densité masque entièrement le calque. Ces cas
réinitialisent correctement la base d’écrêtage quand nécessaire. Un masque blanc
uniforme ou de densité nulle est ignoré avant lecture, contour progressif et calcul
des marges. Les masques peints restent traités normalement et un alpha natif de
1/65 535 n’est pas assimilé à zéro. Le composite de sortie reste une image native
complète lors d’un rendu intégral ; cette étape évite les entrées temporaires inutiles.

L’application d’un masque à densité maximale multiplie désormais directement
les quatre canaux prémultipliés 16 bits par la couverture 8 bits, avec arrondi
entier exact. Les densités partielles utilisent une table de facteurs flottants,
sans quantification intermédiaire et en gardant l’ordre des opérations antérieur.
Cela accélère les masques, écrêtages et sources de fréquences qui emploient ce
chemin CPU ; ni les dimensions de travail ni les tampons natifs ne changent.

La préparation des retouches capture maintenant les `PixelPlane` immuables,
sans convertir les deux sources en QImage dans la GUI. Le worker matérialise les
plans nécessaires ; les deux bandes conservent la même identité de source quand
elles partagent leurs tuiles. Le cache peut donc réutiliser la bande basse selon
son budget. Les leases disque restent possédés jusqu’à la fin du travail.
Les grandes images des bandes et leur intégration finale restent des coûts distincts ;
ce changement réduit la soumission, sans rendre tout le flou régional.

`PixelPlane::uniform` utilise le constructeur natif de `TiledImage` et conserve
les bits prémultipliés, l’espace couleur, le DPR et la résolution du pixel fourni.
Chaque nouveau plan logique reçoit une version distincte de celles des autres
plans et des écritures régionales. Une copie garde sa version jusqu’à une véritable
modification ; une écriture identique n’alloue pas de tuile. Les buffers des filtres
et la sérialisation peuvent toujours matérialiser ce plan logique complet.

La lecture d’un projet mémorise tous les plans déjà décodés avec une clé SHA-256
des octets PNG et leur format natif cible, après validation Base64 et dimensions.
Deux PNG strictement identiques au même format partagent le `PixelPlane` et ses
leases : images de calques dupliqués, masques et sources de fréquences, y compris
avec leur image originale. Un PNG utilisé à la fois en couleur et comme masque
reste converti en deux formats distincts. La table ne conserve pas de copie
supplémentaire des chaînes Base64 et disparaît après chargement. Une retouche ne
détache que les tuiles touchées, sans changer les copies ni leur historique.
La réservation logique compte toujours chaque plan de calque avant décodage,
même partagé ; le budget de préparation suit les blocs physiques uniques.

Ce partage restaure aussi la reconnaissance des paires intactes : avant, les deux
sources rechargées recevaient des identités distinctes et le composite ne pouvait
plus les remplacer par leur source commune. Sur une source transparente, le
repli par deux bandes changeait alors l’alpha. La paire rechargée garde désormais
le composite d’avant sauvegarde. Les PNG différents ou corrompus ne sont pas
assimilés sur la base du seul identifiant de paire.

Lors de l’import initial d’un plan natif, le fond compact est choisi parmi les
couleurs des tuiles entièrement uniformes, pondérées par leur allocation native
(y compris les bords incomplets). Les autres tuiles gardent leurs bits exacts.
Un remplacement d’un plan compatible conserve son fond, ses versions inchangées
et ses leases disque. Les fonds prémultipliés sont étendus par copie native, sans
conversion QColor. Les métadonnées couleur, résolution et DPR sont conservées.
Cela évite de réallouer tout un masque blanc après quelques retouches lors de la
réouverture d’un projet ; le décodage PNG matérialise encore un plan temporaire.

Le placement d’un calque importé construit un fond transparent compact et peint
uniquement l’intersection de l’image centrée avec le canevas, avec une marge de
rasterisation. Les images de taille identique/DPR 1 gardent le chemin de conversion
directe ; les autres conservent les coordonnées et le DPR du rendu QPainter
précédent. Le résultat est transféré comme `PixelPlane`, sans conversion implicite
vers une image complète lors de la construction du calque. Le codec source peut encore demander un plan entier.

Les conversions entre les formats natifs convertissent le pixel de fond puis
chaque tuile stockée, avec vérification d’annulation entre les tuiles. Un format
inchangé garde ses versions et ses mappings ; une conversion réussie obtient de
nouvelles versions. Échec ou annulation laisse le plan source intact. Les masques
ne changent pas de profondeur lors des conversions du document. Les conversions
non natives gardent le repli QImage. Changement de profondeur et promotion à
l’import préparent la nouvelle pile avant d’appliquer une unique étape d’historique.
Les plans actuellement actifs et leurs anciennes versions restent comptés dans
leurs budgets respectifs ; aucun tampon natif de canevas complet n’est nécessaire
pour convertir les formats natifs stockés par tuiles.

Le worker capture le budget RAM réglé dans Cache de travail. Pendant les conversions
et les imports, les blocs des plans préparés sont comptés sans doublonner leurs
versions partagées. Si ce sous-ensemble dépasse le budget, le plan courant est
déchargé sur disque avant publication. L’échec du cache évite de nouvelles tentatives
pour les plans suivants et conserve les pixels en RAM ; il ne transforme pas une édition
valide en erreur. Les clés, mappings et leases restent valides après nettoyage.
Ce contrôle intervient entre plans : le plan en conversion, le décodeur source et
les anciennes versions d’historique peuvent encore s’ajouter temporairement. Le
budget global de la pile et de l’historique reste géré par le worker de déchargement
après une pause ; ces nombres ne sont pas une limite du RSS ou des pages mappées.
Les helpers synchrones ne font pas ces écritures disque sur le thread GUI.

L’ouverture asynchrone applique aussi le contrôle entre plans décodés : chaque
PNG de projet est scellé et peut être déchargé avant le décodage du suivant ; une
image ordinaire peut également être déchargée avant son application. Les plans
et le cache qui porte leurs mappings sont transmis ensemble au document affiché.
Si la configuration du cache a changé entre-temps, la nouvelle reste active et
les leases des anciens mappings préservent les pixels. Un cache indisponible garde
les plans en RAM sans empêcher l’ouverture. JSON complet et plan courant du codec
restent hors de ce sous-budget ; ce mécanisme ne limite pas à lui seul le RSS.

Les déchargements vérifient désormais un jeton entre chaque tuile, et les tâches
du cache sont annulées lors d’un changement de document, de dossier de cache,
de budget RAM ou à la destruction du document. Un appel d’écriture/mapping
courant peut finir ; aucune nouvelle tuile n’est démarrée après le prochain
contrôle. Les mappings déjà créés gardent leurs leases, pixels et identifiants de
versions. La préparation des conversions/imports/décodages transmet aussi son
jeton, afin de ne pas finir inutilement le plan entier après une annulation.

La collecte du déchargement capture sur la GUI uniquement les vecteurs de calques
partagés de l’état actif, d’Annuler/Rétablir et de la source de forme d’historique.
Le worker parcourt ces groupes immuables, ignore plans nuls/compacts et compte les
blocs uniques. L’ordre de préférence des anciennes versions reste identique.
Une modification après capture produit de nouvelles versions sans changer le
travail en attente : une tuile nouvellement peinte reste en RAM jusqu’à la
prochaine collecte, tandis que les anciennes versions peuvent être déchargées.

Le worker RAW développe le capteur puis prépare un snapshot complet : tuilage,
centrage si import, promotion éventuelle des anciens plans et déchargement selon
le budget capturé. L’image QImage du décodeur est libérée avant de transmettre le
résultat ; `retainedDecoderBytes` reste donc nul pour les résultats terminés.
L’ouverture seule ne capture pas les anciens plans du document sur ce worker.
L’import capture l’état initial et crée une seule étape d’historique lors de la
validation GUI, avec profondeur et masques restaurables. Le cache des mappings
suit l’ouverture comme pour les autres formats. Annulation, session ou état devenu
caduc rejette le snapshot préparé. Les allocations internes courantes de LibRaw
et son image développée restent nécessaires pendant le calcul et hors du budget
des tuiles ; certains appels peuvent finir après la demande d’annulation.

Les tâches qui ouvrent un fichier de remplacement ne capturent pas les anciens
plans du document ; seuls les identifiants de session/état restent nécessaires
pour rejeter un résultat périmé. Après annulation et remplacement du document,
ces tâches ne prolongent donc pas la vie de ses tuiles. Les éditions, sauvegardes
et exports conservent leur capture indispensable. Le répartiteur force cette
capture pour les éditions, même si un appelant lui demandait de l’omettre.

La pipette en mode calque actif lit uniquement le carré prélevé dans les tuiles,
au plus 101 × 101 pixels, sur les pixels 8/16 bits et les masques. Les plans
compacts restent prélevés sur leur pixel natif. Les moyennes pondérées par alpha
sont inchangées, sans création d’historique. La source composite conserve ses
règles de cache/proxy. Si aucun rendu terminé n’est disponible, elle compose
uniquement la zone prélevée, avec la somme des halos des filtres spatiaux, sans
remplacer le cache d’affichage ni effacer ses zones invalidées. Le moteur est
commun au rendu complet et régional. Les pipettes de Courbes et les lectures de
couleur des réglages utilisent aussi cette voie pour leur pixel natif, avec les
mêmes coordonnées bornées aux limites du document. Dissoudre, bruit, mosaïque,
fréquences, prévisualisations provisoires et plans à DPR non unitaire conservent
le repli complet pour préserver leurs coordonnées et pixels.

Le contour progressif d’un masque natif est flouté uniquement sur la région
nécessaire, augmentée du rayon de voisinage puis recadrée. Les vrais bords du
document conservent la même extension que le flou complet. Cette voie sert aux
petites recompositions après retouche et aux lectures régionales des outils ;
un rendu intégral calcule toujours le masque intégral. Densité, précision des
couleurs et masques compacts restent identiques. Les contours progressifs peuvent
donc aussi être lus régionalement ; leur affichage de pile reste sur le repli CPU.

Copier une sélection compose uniquement son rectangle englobant, avant le même
écrêtage par sa forme et sa couverture progressive. Les pixels natifs du
presse-papiers gardent leur profondeur. Sans sélection, la copie conserve
l’image complète ; une pile non locale peut aussi nécessiter le repli complet.

Le tampon de duplication conserve une pile native immuable au début du geste pour
les modes « calque actif et inférieurs » et « tous les calques ». Seules les
régions source nécessaires sont composées ; un chevauchement source/destination
ne réinjecte pas les pixels fraîchement peints. La pile est libérée à la fin,
à l’annulation et au remplacement du document. Les piles non compatibles avec
le rendu local gardent un composite complet, réutilisé pendant le geste. La
définition d’un motif prélève aussi uniquement sa sélection, limitée à 1 024².

Le Doigt en mode « tous les calques » réutilise également une pile figée ; la
prise de couleur lit des carrés de la taille de la brosse, avec transparence hors
canevas. L’accumulation de peinture dans la brosse reste identique. Son ancien
composite complet permanent pendant le geste disparaît pour les piles compatibles.
L’édition des pixels reste limitée aux documents 8 bits, les masques aux profondeurs
déjà prises en charge ; cette optimisation n’ajoute pas une conversion silencieuse.

Le Pinceau mélangeur partage la même stratégie pour « tous les calques » : une
pile figée, des lectures à la taille de la brosse et le même état de réservoir.
Son prélèvement Option/Alt lit un seul pixel en mode couleur unie, ou l’empreinte
de brosse en mode texturé, sans reconstruire puis convertir tout le composite
en plan tuilé. Les couleurs moyennées et le comportement des masques restent
identiques. Les replis complets des piles non locales sont conservés.

Les deux correcteurs utilisent aussi une source native figée sans composer le
canevas complet à la préparation. Le Correcteur lit ses rectangles de texture
et de référence ; le Correcteur localisé lit son voisinage borné de recherche,
puis les rectangles de la correction choisie. Les modes de source et l’option
ignorer les réglages conservent leur filtrage de pile. Les piles non locales
gardent un rendu complet paresseux, réutilisé pendant le geste. Les sources sont
libérées à la fin et à l’annulation ; la limitation actuelle aux pixels 8 bits
reste inchangée.

Sélection rapide lit uniquement sa fenêtre de recherche, aussi bien sur le calque
actif que sur une pile compatible. La conversion de comparaison en 8 bits et la
table des pixels visités ont la taille de cette fenêtre. Les coordonnées sont
replacées dans le document avant combinaison et amélioration des bords ; les
modes remplacer/ajouter/soustraire/intersection et l’annulation sont inchangés.
Un calque uniforme n’est étendu qu’à la taille de la fenêtre. Une très grande
brosse peut toujours demander une recherche couvrant une large partie du document.
La Baguette magique garde sa portée globale, mais lit désormais les tuiles à la
demande. La comparaison utilise les mêmes canaux droits 8 bits que la conversion
intégrale précédente, y compris pour les documents 16 bits. Un cache retient une
bande horizontale (au plus 16 Mio pour 16K de largeur), sans image de comparaison
pleine taille. Les bits de visite sont alloués par tuile touchée : 8 Kio par tuile
256² ; le mode non contigu n’en utilise pas. Le parcours garde les bandes de région
dans leur ordre initial pour préserver l’égalité des sélections et leur historique.
Les images natives de lecture et les replis complets des piles non locales restent
à compter séparément. Une sélection complexe peut aussi nécessiter une grande
région géométrique finale.

Les sources natives uniformes reconnues (fond compact ou plan tuilé sans tuile
explicite) donnent directement une sélection pleine surface. Une pile composée
uniforme de calques pixel est aussi admissible, avec opacité, écrêtage et masques
uniformes. Fondu, réglages et autres sources potentiellement spatiales restent
sur le parcours normal. Cette décision lit seulement les métadonnées des plans ;
elle ne cherche pas à prouver l’uniformité de calques denses.

À partir du seuil partagé de quatre millions de pixels, la Baguette magique
capture les versions natives et calcule sur worker. Un composite natif complet
encore courant est réutilisé par partage ; le worker ne reconstruit une pile que
si ce cache n’est pas disponible. La sélection précédente
reste affichée jusqu’au résultat, avec l’indicateur et l’annulation existants.
La session et l’identité d’état sont contrôlées avant application ; annulation,
nouveau geste ou document remplacé empêchent toute publication périmée. Les
options du clic sont figées, sans écraser les préférences modifiées pendant le
calcul. Une seule étape d’historique de sélection est ajoutée, sans marquer les
pixels enregistrés comme modifiés. La capture conserve les leases des plans
nécessaires ; un appel natif de composition en cours peut finir avant le prochain
contrôle d’annulation entre lignes/tuiles.

Les cinq retouches locales lisent le snapshot natif par région, avec quatre pixels
de marge pour Flou/Netteté. Leur couverture est aussi tuilée ; un geste de 12 px
sur un document 24 Mpx/16 bits alloue au plus 512 Kio de couverture et une nouvelle
tuile de 512 Kio, sans reconstruire le calque. La lecture de l’alpha de couverture
respecte désormais les 16 bits au lieu d’interpréter ses lignes en pixels 8 bits.

Œil rouge lit uniquement sa zone de recherche locale dans les tuiles du calque
actif, puis remplace le rectangle des pixels corrigés. Le clic et le cadre gardent
la même recherche de composante rouge, les reflets et les réglages de pupille.
L’outil couleur reste limité aux documents 8 bits.

Le remplissage des tracés de Plume lit et remplace uniquement leur emprise
intersectée par la sélection, sur pixels 8/16 bits comme sur masques. Le masque
de sélection progressif est recadré à cette région. Le rendu conserve la profondeur
native ; le changement d’origine du rasteriseur peut modifier légèrement
l’anticrénelage d’un bord de courbe, sans agrandir les tampons au document entier.

Les six formes géométriques relisent uniquement leur emprise dans un snapshot tuilé,
avec une marge couvrant le contour et ses jointures. Chaque aperçu repart de la
source et invalide l’union des anciennes/nouvelles emprises, sans recopier le calque.
Le retour au point de départ retire l’aperçu et un éventuel calque vide. Le dégradé
sélectionné suit le même chemin ; les calques denses et le dégradé pleine image
restent complets. Une petite forme sur 24 Mpx/16 bits ajoute au plus une tuile de 512 Kio.

Déplacement avec une sélection prépare séparément les régions source et destination.
Le masque progressif et le mode copie Alt gardent le même mélange natif. Un saut
lointain n’alloue pas le rectangle intermédiaire et chaque aperçu repart du snapshot
original. L’invalidation du composite couvre encore une emprise rectangulaire commune ;
les calques de pixels denses gardent le rendu complet.

Sans sélection, les calques peu remplis sur fond transparent se déplacent aussi par
tuiles : les zones absentes ne sont pas allouées et les pixels natifs des tuiles
présentes sont copiés aux coordonnées décalées. Le chemin s’applique avec au plus
un quart des tuiles présentes (ou huit sur les petits documents), sans ratio de
pixels autre que 1. Les images denses et fonds opaques gardent le chemin complet.
Le texte suit son rasteriseur régional dédié. Une petite marque sur 24 Mpx reste de l’ordre d’une tuile
couleur, y compris après un saut de plusieurs milliers de pixels. La copie Alt
notifie l’ajout de calque, et les anciens aperçus sont invalidés.

Les calques de texte sont rasterisés dans leur emprise utile puis écrits dans un
plan tuilé transparent. La mise en forme, le déplacement et le redimensionnement
ne créent plus systématiquement un QImage natif de la taille du document.
Le déplacement garde le snapshot tuilé initial, restaure celui-ci à l’ancre et
invalide les anciennes/nouvelles emprises. La marge conserve les débordements de
police et de ligne de base ; un paragraphe garde son rectangle de découpe. Un texte
hors canevas reste compact et éditable. Un texte couvrant tout le document peut
encore demander un tampon aussi grand que celui-ci.

Le pot de peinture parcourt la composante contiguë avec des vues de tuiles natives
partagées et une couverture de visite parcimonieuse de 256 × 256 en niveaux de
gris. Une tuile absente se lit comme son pixel de fond uniforme ; les vues gardent
leurs leases disque et une écriture dans une copie QImage ne modifie pas la source.
Le mélange ne prépare que l’emprise remplie de chaque tuile, puis applique toutes
les écritures à une version privée avant de remplacer le calque. Il n’alloue plus
de masque ni de tampon couleur complet pour une petite zone fermée.
La contiguïté, les masques, la sélection progressive, l’opacité et les pixels 16 bits
sont conservés. La composante modifiée invalide localement le composite et la vignette.
À partir de quatre millions de pixels dans l’emprise de recherche, le remplissage
est délégué au worker commun : source immuable, progression, annulation pendant
le parcours ou l’écriture et validation atomique en une étape d’historique.
Les petites emprises restent synchrones. Un nouveau geste de modification annule
l’ancien calcul dès la pression, avant même le premier déplacement.

La sauvegarde `.pcr` v9 écrit les métadonnées JSON et les PNG en Base64 directement dans
le fichier atomique temporaire, un plan après l’autre. Elle ne conserve plus les chaînes
Base64 de tous les calques ni une seconde copie JSON complète. Elle ne construit plus
non plus de tampon PNG compressé ou de grande chaîne UTF-16 intermédiaire pour chaque
plan. Pour les versions de plans répétées, un cache LRU local à la sauvegarde
retient leurs payloads Base64 jusqu’à 8 Mio au total. Une répétition en cache écrit
ces octets par blocs de 16 Kio, sans reconstruire de QImage ni réencoder le PNG.
Seules les versions présentes plusieurs fois sont candidates. La capture courante
est elle aussi limitée à 8 Mio ; cache et capture peuvent donc retenir ensemble
jusqu’à 16 Mio de payloads, hors capacité de l’allocateur. Un dépassement abandonne
la capture sans tronquer le flux et marque la version pour éviter les captures
suivantes inutiles. Les plans uniques et les gros payloads gardent le flux direct.
Le convertisseur produit au plus 16 Kio de Base64 à la fois et garde au plus deux
octets en attente. La limite de 256 Mio porte sur le flux réel, y compris métadonnées et
padding ; un échec d’écriture/encodage laisse le fichier précédent intact. Sur une sonde
isolée de douze plans synthétiques de 1 Mpx, le pic RSS de l’encodeur passe de 324 à 74
Mio en 8 bits et de 692 à 122 Mio en 16 bits, avec sorties identiques. Ces mesures
locales n’incluent pas le composite ni l’historique de l’application, et portaient
sur des plans uniques, sans le nouveau cache de répétitions. Sur dix copies d’un
dégradé 4 Mpx/16 bits, la sauvegarde passe localement de 2,78 à 0,285 s, avec un
payload répété de 489 136 octets et des champs PNG/Base64 identiques. L’encodage PNG
peut encore matérialiser un plan natif complet et ses buffers de conversion. La
lecture parse encore tout le JSON ; les grandes sauvegardes utilisent le worker
décrit ci-dessous.

Le dialogue d’export des grandes images utilise un snapshot dans le worker commun.
Le composite natif et les conversions du codec restent nécessaires, mais leur
allocation et leur encodage ne bloquent plus l’interface. Ce traitement est en
lecture seule : il ne restaure pas de calques, n’ajoute pas d’historique et ne marque
pas le document comme enregistré. L’annulation rejette le fichier temporaire avant
le point de publication atomique ; cette dernière phase est ensuite non annulable.

La sauvegarde des grands projets conserve les plans du snapshot par références
tuilées et écrit le flux sur le worker. Le document reste modifié et conserve son
ancienne URL jusqu’à l’accusé de réussite après publication. L’annulation entre
les phases/avant publication conserve le fichier précédent ; le codec courant
peut encore terminer. La fermeture attend la confirmation et ne se poursuit pas
en cas d’erreur ou d’annulation. L’historique et les réglages d’outil sont conservés.

Les ouvertures `.pcr` depuis Ouvrir, les récents et la ligne de commande passent
par un worker annulable, y compris les petits fichiers très compressibles. JSON,
PNG et tuilage sont validés avant de remplacer le document sur la GUI. Le nouveau
projet devient propre, avec URL, profondeur, calques, texte, tracés et tranches ;
les réglages d’outil restent ceux de l’utilisateur. Le helper booléen synchrone
reste disponible pour les tests et les miniatures déjà calculées hors GUI.
Les appels Qt en cours (lecture/parse/codec) peuvent finir après l’annulation ;
leurs résultats sont rejetés. Le JSON complet, le plan natif décodé et les deux
documents peuvent temporairement coexister : ce changement évite le blocage de
l’interface mais ne promet pas une baisse du pic RSS.

L’ouverture des images ordinaires utilise aussi un snapshot de remplacement sur
le worker. Le codec applique l’orientation, puis les conversions sRGB et de format
natif sont réalisées avant de créer les tuiles. Seul le résultat courant devient
un nouveau document modifié, sans URL de projet ni ancien historique. Les RAW
conservent leur développeur dédié.

L’import comme calque passe aussi par le worker commun : décodage, centrage à la
taille du document et promotion éventuelle des calques/sources vers 16 bits. Les
masques existants sont conservés. Une fin valide applique une seule étape ;
Annuler remet la pile, la profondeur et l’état enregistré précédents. Une
annulation, un nouveau document/geste ou une destruction rejette le résultat.
Les vérifications de limites de calques/mémoire restent en place, avant lancement
et après identification de la profondeur native. Le codec ou la conversion du
plan courant peut finir après annulation, sans publication partielle.

Après une pause de 500 ms, un worker décharge les tuiles si leur budget RAM est dépassé
(256 Mio par défaut, réglable de 0 à 4 096 Mio dans Cache de travail). Les états
historiques sont parcourus avant les plans actifs. Les versions en cours d’utilisation
restent valides grâce aux références immuables et aux leases des fichiers. Un refus
d’écriture garde les pixels en mémoire ; vider le cache ne détruit pas les tuiles
encore utilisées. Les limites de dimensions/calques et la réservation logique de
2 Gio ne sont pas relevées.

Les plans d’un pixel restent rééditables après leur première écriture tuilée :
la réexpansion et les clés de rendu utilisent le pixel courant, avec ses bits
natifs et ses métadonnées, plutôt que l’ancien tampon libéré.

Les clés de régions partielles réutilisent au plus huit empreintes SHA-256 par
version de tuile et 256 empreintes de fond par thread. Ces petites métadonnées
avec leurs conteneurs s’ajoutent au comptage des blocs de pixels ci-dessous ; les
régions entièrement couvertes utilisent toujours leur identifiant de version.

`memoryUsage()` compte les versions de tuiles partagées une fois et expose
`tileHeapBytes` pour leurs allocations natives. Ce chiffre ne représente pas toute
la RAM du processus : mappings, composite, sélections, images exportées et buffers
temporaires restent distincts. La RAM résidente des pages mappées est gérée par l’OS.

Les tests vérifient un trait et un masque local sur 24 Mpx/16 bits avec au plus
576 Kio de plans persistants, ainsi qu’édition, déchargement, vidage du cache,
Annuler/Rétablir et sauvegarde/rechargement d’un document 16 bits.

Les conversions vers un format déjà actif et la réaffectation du même espace
couleur ne matérialisent plus le plan complet. En particulier, importer une image
n’ouvre plus un buffer natif pour chaque calque existant resté à la même profondeur.
Le test sur un plan mappé vérifie que ces opérations ne créent aucun bloc RAM et
ne changent pas sa version ; une conversion effective conserve son chemin natif.

L’import d’une image à la taille exacte du document et au ratio de pixels 1
réutilise aussi le plan décodé, après conversion éventuelle à la profondeur de
travail. Il n’alloue plus une image vide complète pour la repeindre à l’identique.
Les imports centrés ou à ratio différent gardent leur chemin de composition.
Les pixels transparents natifs sont comparés en 8/16 bits et l’annulation est vérifiée.

La lecture de projet scelle désormais chaque plan PNG en tuiles avant de décoder
le suivant. Elle ne retient plus une pile de buffers PNG complets en parallèle
avec les plans importés. La validation de tous les calques et tracés précède
toujours le remplacement du document courant ; le format du fichier ne change pas.

L’ouverture RAW consomme le résultat du worker par déplacement et vide son stockage
de résultat. Le décodeur ne conserve ainsi plus une image native complète après
import en tuiles, ni après annulation ou erreur. `retainedDecoderBytes` expose les
octets d’un résultat terminé encore retenu ; ce compteur n’inclut pas les buffers
de travail du décodeur pendant son exécution.

Les tampons de duplication et de motif écrivent aussi des régions et utilisent
une couverture tuilée, sur pixels comme sur masques. La source du calque actif
reste un snapshot de tuiles pendant tout le geste ; seul le rectangle échantillonné
est lu, avec son décalage fractionnaire et ses bords transparents. Les motifs
restent de petites images répétées. Les modes d’échantillonnage « actif et dessous »
et « tous les calques » préparent encore un composite natif complet par geste.

Le passage d’un plan compact d’un pixel à des tuiles conserve ses valeurs natives,
son espace couleur (y compris absent pour les masques), sa densité de pixels et
son ratio de pixels. Il n’ajoute plus implicitement un profil sRGB à un masque.
La lecture d’une région logique d’un seul pixel fonctionne aussi hors de l’origine
du pixel stocké, sans retourner une zone transparente. Les régions uniformes
conservent également la densité et le ratio de pixels avant toute écriture.

Les écritures régionales comparent les octets natifs avant de détacher une tuile.
Une écriture identique conserve ses identifiants, mappings et versions logiques ;
un patch traversant plusieurs tuiles ne copie que celles réellement différentes.
Les plans compacts restent compacts si la couleur écrite est déjà celle du fond.
Cela évite les nouvelles allocations et les invalidations de textures fondées sur
les versions de tuiles ; les règles d’historique propres aux outils ne changent pas.

Le Doigt lit et remplace uniquement la zone de chaque empreinte dans la destination
tuilée, y compris sur un masque. Sa poche et sa pointe restent limitées à la taille
du pinceau ; il n’alloue pas de couverture du document. La reprise des couleurs
continue à lire la destination déjà modifiée pendant le geste. L’échantillonnage
de tous les calques prépare encore un composite complet une fois ; le traitement
des plans couleur reste limité aux documents 8 bits, comme auparavant.

Remplacement de couleur conserve sa source en tuiles et sa couverture maximale
par pixel dans une couverture tuilée. Chaque empreinte lit et remplace son seul
rectangle ; les règles d’échantillonnage, de tolérance, de contiguïté et de recherche
des contours restent identiques. Cet outil reste limité aux plans couleur 8 bits.

Le Pinceau mélangeur utilise une source figée par tuiles et une destination
régionale sur pixels/masques. Le prélèvement Alt-clic, uni ou multicolore, ne
matérialise plus le calque ; seuls le réservoir et l’empreinte sont préparés.
Les modes de mélange, la reprise et l’épuisement de la charge sont conservés.
L’échantillonnage de tous les calques garde son composite complet par geste.

Les deux correcteurs conservent la source du calque actif par versions de tuiles.
La diffusion lit ses deux empreintes, puis écrit seulement le rectangle réellement
modifié. Le Correcteur localisé lit une région bornée couvrant ses candidats
donneurs, leurs intérieurs et leurs contours, sans reconstruire le plan complet.
Leurs modes composés préparent encore un composite natif une fois par geste.
Le contrôleur n’a plus de couverture pleine image ni de destination pleine image
pour les outils à empreintes ; les outils sans couverture cumulative ne gardent
que leur pointe locale.

La Forme d’historique conserve le plan source par versions de tuiles, même lorsque
son état n’appartient plus à la branche d’historique courante. Couverture, source,
destination et mélange sont régionaux. Les versions mappées restent utilisables
après vidage du cache.

La variante artistique conserve également source, destination et couverture en
tuiles. Une petite région source fournit les couleurs et gradients du groupe de
touches ; la même dispersion déterministe est peinte dans les tuiles concernées.
La tolérance et le mélange restent locaux. Les deux variantes couleur restent
limitées aux documents 8 bits.

Les réductions CPU `FastTransformation` des plans figés lisent directement les
pixels échantillonnés dans les tuiles, au lieu de reconstruire l’image native.
Deux petites bandes de coordonnées Qt conservent ses arrondis par axe et par
format, y compris lorsque les deux dimensions changent. Alpha, profondeur,
espace couleur et ratio de pixels sont préservés. Les agrandissements et les
transformations lisses conservent leur chemin Qt complet.

Sur un plan dense 24 Mpx/16 bits, cinq aperçus 1000 × 667 prennent environ 41 ms
par lecture régionale, contre 102 ms pour reconstruction native puis réduction,
avec les mêmes pixels. C’est une mesure isolée en RAM, pas un gain global de l’UI.

## Tests denses et disque plein — 3 novembre 2026

Le scénario optionnel `denseTwentyFourMegapixelPlanesSurviveOffloadAndReload`
charge quatre images entièrement remplies de 6 000 × 4 000 en 16 bits, soit
768 000 000 octets de plans natifs. Le déchargement conserve 1 536 blocs et ramène
`tileHeapBytes` à zéro ; les mappings conservent ces 768 Mo, dont la résidence
physique est gérée par le système. Un trait recrée moins de 1 Mio de tuiles RAM.
Les versions précédentes, Annuler/Rétablir, sauvegarde et rechargement sont vérifiés.

Première mesure locale de toute cette séquence, avec le document initial et celui
rechargé coexistants : pic RSS 3 359 752 192 octets (3,13 Gio), empreinte maximale
2 562 313 216 octets (2,39 Gio), environ 14 s. Le scénario inclut le décodage PNG,
les copies temporaires d’import, l’encodage du projet et son rechargement. Il
illustre pourquoi le budget des tuiles ne constitue pas un plafond du processus.

Après suppression des conversions sans effet, réutilisation des imports à taille
identique et scellement des plans pendant la lecture, une nouvelle exécution sans
compilation concurrente donne 2 647 834 624 octets de RSS maximal (2,47 Gio),
1 831 045 592 octets d’empreinte maximale (1,71 Gio), environ 13 s. Sur cette
séquence, le pic RSS baisse d’environ 21 % ; la résidence des mappings et la charge
système font varier les mesures. Ce n’est toujours pas un plafond RAM global.


```sh
PHOTOCRAP_DENSE_MEMORY_TEST=1 QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  build/photocrap_tests denseTwentyFourMegapixelPlanesSurviveOffloadAndReload
# macOS : préfixer la commande par /usr/bin/time -l pour les mesures RSS.
tests/run-full-disk-test.sh
```

Le second script macOS crée et sature un volume HFS+ temporaire de 32 Mio, lance
le test, puis démonte et supprime le volume. Le test vérifie que son chemin est
la racine d’un petit système de fichiers marqué ; il refuse de remplir un dossier
ordinaire. Les mappings antérieurs restent lisibles, le nouveau plan conserve sa
tuile RAM après refus, et une nouvelle modification reste possible après vidage
du cache. Ces deux scénarios sont optionnels dans la suite ordinaire, pour éviter
une forte consommation de mémoire ou un montage de volume en CI.

## État historique avant cette migration


État vérifié pour **v0.15**, le **29 octobre 2026**, à partir de l’audit du 28 octobre.
Cette version livre les optimisations de composition, le cache disque des rendus de
fréquences et la couverture des traits par tuiles. Les pixels des calques, les masques
et l’historique restent à migrer ; l’architecture de cette suite est distincte des
fonctions déjà disponibles.

## Ce que les limites actuelles signifient

Le maximum de 512 calques est une limite de structure, pas la promesse de conserver
512 photographies pleine résolution en mémoire. Le budget des plans de calques est
de 2 Gio, avec une limite de document à 32 mégapixels.

Pour une photographie de 6 000 × 4 000 pixels :

| Donnée | Stockage d’un plan complet |
|---|---:|
| Couleur RVBA 8 bits par canal | 96 000 000 octets, environ 91,6 Mio |
| Couleur RVBA 16 bits par canal | 192 000 000 octets, environ 183,1 Mio |
| Masque 8 bits | 24 000 000 octets, environ 22,9 Mio |
| Calque vide / masque uniforme compact | Un pixel, sans plan pleine résolution |

Les réglages conservent des paramètres et un masque ; les groupes ne stockent pas
d’image. Les sources de séparation de fréquences comptent aussi dans le budget.
Les plans non uniformes partagés sont réservés séparément par calque pour anticiper
une modification ou le rechargement du projet, même si QImage partage encore leurs
pixels en RAM.

L’historique utilise le partage des plans inchangés et compte les images supplémentaires,
avec un objectif de 256 Mio et 40 étapes. Il conserve au moins la dernière annulation,
même si elle dépasse ce budget. Les rendus de fréquences utilisent un LRU de 128 Mio ;
le cache des proxies écran a son propre plafond de 128 Mio.

Ces nombres ne sont **pas un plafond global de RAM**. Le composite, les buffers des
outils, les rendus en cours, les sessions de propriétés, le décodage et l’enregistrement
consomment aussi de la mémoire. `memoryUsage()` expose principalement les plans de
calques, l’historique, les couvertures de sélection et le cache de fréquences ; ce
diagnostic n’est pas une mesure de la mémoire résidente du processus.

## Portée des tests

- `hundredsOfLargeLayersStayCompactAfterReload` crée 301 calques sur un document
  24 Mpx en 16 bits : 150 calques vides munis de masques et 150 réglages invisibles, en plus
  du calque initial. Il vérifie les 450 plans compacts, le budget, la sauvegarde et
  le rechargement. Il ne mesure pas la composition de 301 photographies remplies.
- `layerCountLimitAllowsFiveHundredAndTwelveCompactLayers` vérifie la limite de
  structure et le refus d’un 513e calque sans modifier l’historique.
- `paintingMaterializesOnlyTheEditedPlane` vérifie l’allocation du seul plan modifié,
  son retour à l’état compact par Annuler et la conservation de la précision.
- La suite teste une pile de **64 calques réellement remplis**, en 8 et
  16 bits, avec opacités et masques uniformes. Le résultat est comparé à une composition
  QPainter indépendante, y compris après une retouche locale et Annuler/Rétablir.
  À 1 024 × 512 pixels, les plans couleur occupent respectivement 128 et 256 Mio.
- Un test d’éviction du LRU vérifie qu’une bande haute encore en cache est réutilisée
  lorsque sa bande basse a été évincée. Quatre autres cas vérifient les proxies avec
  un calque masqué, un groupe masqué, une opacité nulle ou tous les calques de pixels masqués.

## Optimisations préparatoires

- Composition en mode Normal directement dans l’accumulateur du rendu, au lieu
  de recopier ce composite pour chaque calque.
- Lecture partagée des images, sources et masques lors d’un rendu complet ; seules
  les régions découpées nécessitent une copie. Les sources et l’historique restent
  protégés par le partage implicite et la copie à l’écriture de QImage.
- Retour du résultat du worker sans duplication pleine résolution supplémentaire.
- Pas de réduction ni de conversion des plans invisibles dans les snapshots de rendu,
  y compris les enfants d’un groupe masqué et les calques d’opacité nulle. Des entrées
  vides conservent les limites des chaînes d’écrêtage.
- Consultation de la bande haute du cache avant toute conversion de source ou recherche
  de bande basse. Cela évite qu’un recalcul inutile évince le résultat recherché.

Ces changements réduisent des copies et traitements inutiles. Ils ne remplacent pas
un stockage par tuiles et ne relèvent pas la limite de 2 Gio.

Mesures indicatives sur le Mac de développement arm64, Qt 6.12, pour les 64 calques
remplis de 1 024 × 512 pixels : composition à 38 ms avant / 25–30 ms après en 8 bits,
51 ms avant / 12–20 ms après en 16 bits. Le processus du test 16 bits seul atteint
environ 315 Mio de mémoire résidente maximale, pour 256 Mio de plans couleur.
Ces exécutions locales ne constituent pas un seuil de performance garanti ni une
mesure du pic complet de l’application avec son interface et ses autres caches.

## Cache disque : première étape implémentée

La section **Réglages → Cache de travail** permet de choisir le dossier, fixer un budget
entre 0 et 64 Gio (4 Gio par défaut) ou vider le cache. Zéro désactive le stockage ; les
rendus restent calculables sans lui. Les réglages sont persistants et appliqués à chaud.

- Les bandes de fréquences d’au moins 16 Mio sont stockées par les workers, en précision
  native et sans recompression. Les petits proxies et les demandes de vignettes sur le
  thread d’interface restent en RAM, sans lecture/écriture disque.
- Chaque document et ses workers partagent une session. Un nouveau document reçoit une
  nouvelle session ; les anciennes restent valides tant qu’un worker ou une image s’en sert.
- Le LRU RAM de 128 Mio reste le premier niveau. Lorsqu’une bande n’y est plus, le rendu
  cherche son bloc sur disque avant de recalculer. Une bande trop grosse pour ce LRU peut
  donc quand même être réutilisée depuis le disque.
- Le stockage utilise des blocs immuables avec en-tête versionné, métadonnées couleur,
  lignes natives et SHA-256. QSaveFile publie atomiquement les écritures complètes.
  Les lectures vérifient l’intégrité et renvoient une QImage sur un mapping en lecture
  seule ; une modification de cette image provoque une copie, sans modifier le bloc.
- Le budget compte aussi les fichiers encore retenus par des images après leur éviction
  de l’index. S’ils empêchent une nouvelle insertion, celle-ci est refusée sans perdre le
  rendu. Une réserve de 512 Mio d’espace libre est vérifiée avant chaque écriture.
- Un dossier inaccessible, une écriture impossible ou un bloc endommagé n’interrompt pas
  l’édition : le rendu est calculé et un diagnostic apparaît dans les réglages du cache.
- Vider le cache libère les blocs inutilisés. Les mappings encore utilisés conservent
  leur fichier jusqu’à la fin du rendu. La fermeture nettoie la session après sa dernière
  référence ; les sessions abandonnées sont nettoyées au prochain usage, avec marqueur
  PhotoCrap et verrou d’instance pour protéger les sessions actives et dossiers étrangers.

`scratchCacheUsage()` expose volume disque, blocs indexés, mappings, réutilisations et
dernier diagnostic. Le volume mappé n’est pas une mesure de RAM résidente. Une nouvelle
session ou un changement de dossier peut temporairement coexister avec une ancienne
session encore utilisée : le budget s’applique à chaque session, pas à la somme de ces
transitions ni aux autres applications.

La suite `scratch_cache` couvre formats 8/16 bits et masques, fidélité des octets et des
métadonnées, copie à l’écriture, durée de vie des mappings, LRU, blocs retenus, corruption,
stockage indisponible, réserve d’espace et concurrence avec vidage. Les tests du document
vérifient la réutilisation entre workers de prévisualisation et le repli sans cache ; le
smoke test QML couvre la page de réglages. La réserve insuffisante est simulée dans la suite ordinaire ; le scénario isolé
décrit plus haut vérifie aussi un disque réellement saturé. Les mesures sur
disques lents restent à compléter.

Ce premier usage conserve des rendus recalculables. Il ne décharge pas encore les plans
éditables ni l’historique et ne relève aucune limite du document. Le même stockage de
blocs pourra recevoir des tuiles quand l’accès aux pixels aura été adapté.

Références des contrats Qt utilisés : [QImage et buffers externes](https://doc.qt.io/qt-6/qimage.html),
[mapping de fichiers](https://doc.qt.io/qt-6/qfiledevice.html#map).

## Tuiles : moteur et premier usage dans les traits

`src/storage/TiledImage.*` fournit des plans parcimonieux en tuiles de 256 × 256 pixels,
avec fond uniforme d’un pixel. Les formats pris en charge sont RVBA prémultiplié 8/16 bits
et masque 8 bits. Lire une région ne matérialise que cette région ; les tuiles absentes
restent implicites. Les copies partagent les versions de tuiles inchangées et peindre
crée des versions des seules tuiles touchées. Le mode persistant publie ses modifications
après réussite ; le mode de travail temporaire réutilise les tuiles non partagées.

Le stockage disque de ces versions est également testé : un worker peut remplacer leurs
pixels en RAM par des blocs mappés, avec une référence propre par tuile. Une éviction,
un vidage ou la destruction de l’index du cache ne supprime pas les pixels d’une version
encore utilisée. Une insertion refusée conserve les tuiles restantes en RAM. Cela prépare
le stockage des plans éditables ; les calques du document n’utilisent pas encore ce plan.

Le premier branchement concerne la **couverture des traits de pinceau, crayon et gomme**,
sur les pixels comme sur les masques. Elle est calculée dans la précision du document,
avec débit cumulatif, pointe douce/dure, angle et arrondi. Les tuiles sont gardées en RAM
pendant le geste, sans écriture disque inutile pour ce tampon temporaire, puis libérées
à la fin ou à l’annulation. Une allocation impossible annule le geste.

Sur un masque d’un document 6 000 × 4 000 en 16 bits, un petit trait tenant dans une tuile
réserve **512 Kio de couverture au lieu de 192 000 000 octets**, soit environ 183 Mio.
Cela ne comprend pas le masque lui-même, les pixels du calque ni les buffers du rendu.
`memoryUsage()` expose `strokeCoverageBytes` et `strokeCoverageTiles` pendant le geste.
Le plan éditable et la base du trait utilisent encore des QImage de taille complète.

La suite `tiled_image` vérifie régions et bordures, partage entre versions, sauvegarde
sur disque, lectures après éviction/destruction du cache et concurrence entre édition
et déchargement d’une autre version. La comparaison des gradients Qt autorise un écart
d’un niveau de canal natif dû à la rasterisation avec des origines de découpage différentes ;
la conservation des pixels et des versions est testée exactement. Les tests du document
couvrent les trois outils en 8/16 bits, sur pixels/masques, Annuler/Rétablir, abandon du geste
et la couverture parcimonieuse sur 24 Mpx.

## Étape suivante : plans de calques et historique par tuiles

Un cache d’aperçus seul ne suffit pas : les plans éditables et les versions de l’historique
resteraient en mémoire. La cible est un stockage de travail par tuiles, réutilisé par
les calques, les masques, l’historique et les calculs. Les proxies restent une dérivation
indépendante à la résolution d’affichage.

### Données et partage

- Introduire un plan de pixels avec dimensions, format, profondeur et espace couleur,
  derrière lequel les outils accèdent à des tuiles ; commencer par 256 × 256 pixels
  et confirmer cette taille par mesure.
- Représenter sans allocation les tuiles vides ou uniformes. Un trait matérialise
  uniquement ses tuiles et le voisinage nécessaire à son effet.
- Donner un identifiant immuable à chaque version de tuile. Calques dupliqués et
  historique partagent ces versions ; modifier une tuile crée une nouvelle version.
- Stocker les tuiles couleur en précision native 8/16 bits ; masques et couvertures
  de sélection restent en 8 bits. La réduction d’un aperçu ne remplace jamais ces données.

### Budgets et ordonnancement

- Séparer le budget RAM des tuiles actives, les caches de rendus et le budget disque.
  Exposer dans Réglages le dossier de travail et les budgets, avec contrôle de
  l’espace libre. Déterminer les valeurs par défaut après les essais de charge.
- Épingler les tuiles utilisées par un geste ou un worker : elles ne peuvent pas être
  évincées pendant leur utilisation. Limiter les calculs simultanés et leurs buffers.
- Écrire les tuiles modifiées en arrière-plan avant de pouvoir les évincer ; une tuile
  inchangée déjà sur disque peut quitter la RAM immédiatement. Prioriser les régions
  visibles et le geste en cours.
- Réutiliser les versions de tuiles dans l’historique, avec un budget disque et une
  politique explicite de réduction des anciennes étapes. Ne pas maintenir en parallèle
  des QImage complètes de chaque calque, ce qui annulerait le gain.

### Fichiers et erreurs

- Un espace de travail temporaire par session de document, dans le cache utilisateur
  ou le dossier choisi ; version de format, dimensions et contrôle d’intégrité par bloc.
- Publication atomique d’un bloc et de son index après écriture complète. Une tuile
  ne devient évictable qu’après confirmation de cette écriture.
- Disque plein, dossier inaccessible ou bloc illisible : abandonner l’opération en cours
  sans supprimer la dernière version valide ni déclarer le document sauvegardé.
- Nettoyer les fichiers lorsque leurs dernières références disparaissent, en tenant
  compte des workers et de l’historique. Au démarrage, identifier les sessions abandonnées
  sans supprimer celles d’une autre instance active.
- Le cache reste distinct d’un fichier `.pcr` et ne tient pas lieu d’autosauvegarde.
  Une récupération après crash exige un journal et une stratégie de reprise séparés.

### Migration et validation

1. Introduire les plans/tuiles et leur comptabilité sans modifier les résultats ni
   le format `.pcr` ; importer/exporter ses plans en flux au lieu de tout décompresser
   ou sérialiser simultanément.
2. Porter pinceau, gomme, masques et remplissages sur les tuiles modifiées, puis la
   composition et les filtres, avec gestion des voisinages et de l’écrêtage.
3. Raccorder les plans de tuiles au stockage disque déjà introduit, avec éviction réelle
   de leurs allocations RAM, puis l’historique partagé.
4. Mesurer mémoire résidente et pics d’allocation, latence des traits, temps de rendu,
   volume disque et précision sur des piles remplies en 24 Mpx/16 bits.
5. Tester Annuler/Rétablir après éviction, sauvegarde/rechargement, annulation d’un worker,
   disque plein, écritures interrompues, nettoyage et instances simultanées.

La limite actuelle de 2 Gio doit rester en place jusqu’à validation de cette migration :
la relever avant de sortir réellement les plans de la RAM déplacerait seulement le risque.
