# Migration des gros documents et du rendu

Travail demandé le 2 novembre 2026 : réaliser les cinq chantiers ci-dessous dans l’ordre, avec
commits distincts et validation avant chaque livraison. Les cinq bases sont désormais
implémentées ; les cases cochées décrivent leur périmètre validé. Les essais matériels encore
indisponibles sont laissés explicitement ouverts. Les filtres CPU sont contrôlés
avec Accelerate et avec leur chemin portable ; leurs arrondis préexistants ont des
références séparées et les lectures régionales sont comparées au rendu intégral.

## Suite autorisée le 4 novembre 2026

Progression par optimisations vérifiées et commits distincts :

- [x] Tables natives des réglages par canal dans la pile GPU : Courbes, Niveaux,
  Luminosité/Contraste, Exposition, Balance, Isohélie et Négatif. Table de 256 ou
  65 536 entrées selon la profondeur, cache GUI LRU de 4 Mio, masques et composition
  conservés. Vérifications moteur, logiciel et Metal sur piles transparentes.
- [x] Clés de cache stables pour les tables immuables : aucun transfert ni passe
  sur une pile tonale inchangée, vérifié sur les 18 réglages en Metal 8/16 bits.
- [x] Réglages tonals analytiques dans les piles GPU 8/16 bits : couleurs, matrices,
  luminance et palettes, sans texture de table volumineuse pour les calculs directs.
- [x] Masques progressifs dans la pile GPU : halo, arrondis, densité et
  réutilisation des passes ; sources uniformes sans passe supplémentaire.
- [x] Ouverture `.pcr` indexée, sans agrégat JSON des PNG/Base64, préflight complet
  puis un calque à la fois ; métadonnées réordonnées et JSON corrompu contrôlés.
- [x] Grands exports PNG natifs 8/16 bits et BMP par bandes, flux compressé borné,
  annulation et publication atomique ; pas de composite complet pour les piles locales.
- [x] Composite temporaire transféré au worker d’encodage ; fond blanc JPEG/BMP
  8 bits en place, source 16 bits libérée après conversion, pixels/fichiers inchangés.
  Les codecs JPEG/WebP Qt nécessitent toujours une image complète.
- [x] Déplacement tuilé des calques denses : pixels natifs exacts, copies Alt,
  annulation et fonds uniformes conservés avec seules les bordures matérialisées.
- [x] Dégradé pleine image par bandes natives de 4 Mio maximum (256 lignes au
  plus), sans tampon source/destination complet ; références QPainter exactes
  8/16 bits, opacité, alpha, sens inversé et annulation.
- [x] Entrées GPU réutilisées pour les piles dépassant le budget de textures :
  sources, masques et tables téléversés entre passes, pixels natifs conservés.
- [x] Intermédiaires GPU mutualisés par dimensions : accumulateurs, entrées,
  filtres et passes horizontales des réductions ; sortie indépendante par tuile.
  Comparaisons Metal des zooms, masques, flous et chaînes 8/16 bits.
- [ ] Compacter les différences de tuiles de l’historique.
- [ ] Consolider les sondes de latence/RAM et les validations matérielles disponibles.

## 1. Plans éditables et historique par tuiles

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

- [x] Dispatch de la Baguette magique sur worker pour les grands documents,
  résultat courant seulement, annulation et historique de sélection préservés.
- [x] Baguette magique : lectures natives à la demande, cache de comparaison
  borné à une bande et visite binaire par tuiles, sans conversion globale.

- [x] Pipette sur calque actif/masque limitée à son échantillon natif,
  moyennes et précision identiques aux prélèvements précédents.
- [x] Ouvertures de remplacement sans capture des anciens plans, session/état
  toujours contrôlés ; snapshots requis conservés pour éditions, sauvegarde et export.
- [x] Collecte des plans de l’historique déplacée dans le worker ; la GUI
  capture seulement les groupes partagés, plans nuls/compacts ignorés.
- [x] Déchargements disque annulables entre tuiles ; tâches caduques arrêtées
  sur changement de document, de cache ou de budget, et à la destruction.
- [x] Budget RAM capturé par le worker et déchargement des plans préparés
  pendant conversion/import et décodage des fichiers, avec repli mémoire lorsque
  le disque est indisponible et transmission des mappings après ouverture.
- [x] Conversions natives par tuiles et fond compact, annulables entre tuiles,
  préparation atomique de la profondeur et des promotions à l’import.
- [x] Choix du fond compact par poids des tuiles uniformes à l’import initial ;
  réouverture des plans presque uniformes sans allocation de toutes leurs tuiles.
- [x] Import exact des plans existants dans `TiledImage`, remplacement atomique et
  réutilisation des versions des tuiles inchangées, y compris après déchargement disque.
- [x] Stockage des plans des calques/masques et de leurs versions d’historique dans
  ces plans ; lectures régionales par la composition et les outils.
- [x] Suppression des copies complètes permanentes, déchargement asynchrone des
  tuiles non utilisées, budgets RAM/disque et état de la pression mémoire.
- [x] Validation édition/Annuler/Rétablir/sauvegarde après éviction, disque plein,
  documents chargés en 8/16 bits et mesures du pic réel de RAM.

Les calques, masques et sources de fréquences utilisent `PixelPlane`. Les outils à empreintes
(pinceaux, retouches, tampons, formes d’historique et correcteurs) lisent et écrivent des
régions directement ; leurs sources composées et les autres outils peuvent encore matérialiser
un plan temporaire. Les formes géométriques et les dégradés sélectionnés prévisualisent aussi
leur seule emprise. Le placement des images importées est également régional, sur fond
transparent compact. Le déplacement d’une sélection traite séparément sa source et sa
destination ; les calques natifs, denses ou parcimonieux, se déplacent par tuiles et le texte est
rasterisé dans son emprise utile. Les dégradés pleine image parcourent des bandes natives bornées. Les tuiles identiques gardent leurs versions et leurs mappings lors des écritures
régionales. Le budget de 256 Mio par défaut est un objectif de stockage des tuiles, pas un
plafond RAM global. Le composite complet CPU, les sélections et les buffers temporaires
restent à compter séparément. Le pic de RAM est mesuré sur des gestes localisés et sur quatre
plans remplis de 24 Mpx en 16 bits. Un volume temporaire de 32 Mio réellement saturé vérifie
le refus d’écriture sans perte des versions ; les disques lents restent à mesurer.

- [x] Séparation de fréquences utilisant directement les tuiles natives non
  masquées, partagées par les deux bandes et immuables après retouche de l’original.
- [x] Sources figées des deux correcteurs : lecture des rectangles ou voisinages
  de recherche, modes actif/inférieurs/tous et exclusion des réglages préservés.
- [x] Copie de sélection limitée à son rectangle, forme et couverture progressive
  conservées dans les pixels natifs du presse-papiers.
- [x] Contour progressif des masques flouté sur la région utile avec son halo,
  pixels du rendu complet conservés en 8/16 bits.
- [x] Sélection rapide : lecture, conversion de comparaison et table de visite
  limitées à la fenêtre explorée, coordonnées et combinaison préservées.
- [x] Pipettes de Courbes et lectures de couleur des réglages limitées au pixel
  natif, précision et cache d’affichage conservés.
- [x] Pinceau mélangeur : pile figée par geste et prélèvement Option/Alt régional
  (pixel ou texture), réservoir et moyennes conservés.
- [x] Prise de couleur régionale du Doigt sur pile figée, couverture et mélange
  conservés aux bords et sur masques.
- [x] Sources composées du tampon figées par geste, lecture des seules régions
  source et définition régionale des motifs ; replis complets réutilisés.
- [x] Lecture native régionale d’une pile compatible pour la pipette, halos
  cumulés et cache d’affichage intact ; repli complet des effets non locaux.

## 2. Composition de la pile sur GPU

Premier chemin disponible : tuiles visibles de 512 px, calques de pixels, texte, Couleur unie,
masques sans contour progressif, visibilité des groupes et écrêtage. Les clés des bordures
partielles dépendent uniquement de leurs pixels natifs, avec des empreintes réutilisées et
bornées ; une retouche voisine n’invalide plus une bordure inchangée. Les tuiles entières
gardent leurs identifiants rapides. Les 21 modes pris en charge utilisent RGBA8 pour les
documents 8 bits et RGBA32F pour les intermédiaires 16 bits ; les couvertures sont en R8. Les
versions régionales conservent les textures et composites de tuiles inchangées. Le budget du
jeu de travail visible est vérifié avant allocation. Les réglages tonals, les modes
Dissoudre/Mélange maximal/TSL et les contours progressifs utilisent encore le composite CPU,
ainsi que les backends sans formats requis. Ce rendu concerne l'affichage ; les exports et
projets gardent leur référence native CPU.

- [x] Composition des calques pris en charge par tuiles, masques et modes de fusion,
  invalidation régionale et conservation des intermédiaires utiles.
- [x] Choix explicite des formats/précisions intermédiaires, références CPU et
  replis pour les combinaisons non prises en charge et le backend logiciel.
- [x] Vérification des groupes, écrêtages, transparences, ordre des effets,
  redimensionnement, recréation de fenêtre/graphe et limites de texture.
- [ ] Essais physiques de perte de périphérique et backends Vulkan/Direct3D sur
  leurs plateformes ; ces mesures ne sont pas disponibles sur ce Mac.

## 3. Filtres spatiaux sur GPU

- [x] Flous de boîte, gaussien et directionnel, netteté et passe-haut dans la pile
  GPU, avec halos cumulés et passes réutilisées. L’affichage est accéléré ; le
  rendu final natif reste la référence CPU en 8/16 bits.
- [x] Aperçu GPU pendant le blocage du worker CPU, annulation immédiate et rejet
  des rendus CPU périmés ; comparaisons aux rayons 0/1/8/31/100 et aux frontières
  horizontales/verticales, avec alpha et masques, en 8/16 bits.

## 4. Généralisation de l’asynchrone

- [x] Inventaire des opérations synchrones restantes et snapshots immuables.
- [x] Transformations, opérations complètes de calques et filtres directs : progrès,
  annulation, validation atomique et unique étape d’historique.
- [x] Tests de fermeture/changement de document pendant une opération, erreurs
  de ressources et priorité aux interactions/aperçus récents.

Le répartiteur commun couvre redimensionnement, rotations/miroirs, recadrage de sélection,
profondeur depuis le menu, fusion vers le bas, remplissages/effacement, inversion des masques,
pot de peinture contigu et filtres directs. À partir de quatre millions de pixels traités, un
worker de faible priorité calcule sur un snapshot ; seule la fin valide applique une étape
d’historique. Échap, Annuler ou Annuler/Rétablir rejettent la demande. Le document peut être
fermé ou remplacé sans attendre le worker. Un appel Qt non interruptible peut terminer son
plan courant après l’annulation ; son résultat n’est jamais appliqué. Les modifications
compactes restent synchrones. L’ouverture RAW garde son worker spécialisé, qui prépare
maintenant aussi les tuiles et l’import avec centrage, promotion et cache avant validation
GUI. Le recadrage interactif, les sélections et la retouche de fréquences conservent leurs
workers spécialisés. L’export des grandes images utilise maintenant le même répartiteur en
lecture seule, sans restaurer un snapshot dans le document ni créer d’historique. Les
sauvegardes des grands projets utilisent aussi ce mode, avec confirmation de l’état enregistré
après publication. L’ouverture des projets décode et valide tous les plans sur le worker ;
seul un résultat encore courant remplace le document, sans historique ancien ni nouvelle
modification. Le décodage, l’orientation et la conversion sRGB des images ordinaires passent
aussi par le worker ; leur document est modifié sans URL de projet, comme auparavant. L’import
comme calque utilise le mode édition du worker, y compris le centrage et la conversion des
plans lors d’une promotion 8→16 bits ; une fin valide crée une seule étape d’historique. La
sauvegarde v9 écrit désormais ses PNG/Base64 et son JSON en flux atomique pour éviter
l’accumulation du projet encodé ; un LRU de 8 Mio réutilise les petits payloads
répétés, avec une capture bornée et un repli en flux. Le codec peut encore demander
un plan natif complet sur un cache miss.

## 5. Niveaux de détail GPU et budget graphique global

- [x] Pyramide de réduction conservée sur GPU, raccords et phase d’échantillonnage
  vérifiés contre les références actuelles.
- [x] Comptabilité partagée des textures photo/masques/intermédiaires/effets,
  épinglage des ressources en cours d’utilisation et éviction des autres.
- [x] Mesures locales de soumission de trame, tailles de textures et fidélité sur
  Metal/OpenGL ; repli sous petit budget et reconstruction après libération du graphe.
- [ ] Mesures de VRAM physique du pilote et validation Vulkan/Direct3D.

Les tuiles des piles GPU éligibles conservent les réductions 1/2, 1/4, 1/8 et 1/16 sur GPU.
Leurs deux passes d’aire partent du composite natif de la tuile, avec une grille globale au
document, y compris pour les dimensions impaires. Elles ne remplacent pas le calcul natif des
filtres ou les exports. Le jeu de travail inclut toujours la tuile native et ses entrées : si
cette réserve ne tient pas, si le zoom demande un niveau plus petit, ou si la pile est
incompatible, l’affichage réduit conserve son chemin CPU. Il n’y a pas de promesse de
réduction GPU de toutes les piles ni de plafond sur la VRAM totale du pilote.