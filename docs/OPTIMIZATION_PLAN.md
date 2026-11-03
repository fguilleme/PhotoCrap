# Migration des gros documents et du rendu

Travail demandé le 2 novembre 2026 : réaliser les cinq chantiers ci-dessous dans
l’ordre, avec commits distincts et validation avant chaque livraison. Les cinq
bases sont désormais implémentées ; les cases cochées décrivent leur périmètre
validé. Les essais matériels encore indisponibles sont laissés explicitement ouverts.

## 1. Plans éditables et historique par tuiles

- [x] Import exact des plans existants dans `TiledImage`, remplacement atomique et
  réutilisation des versions des tuiles inchangées, y compris après déchargement disque.
- [x] Stockage des plans des calques/masques et de leurs versions d’historique dans
  ces plans ; lectures régionales par la composition et les outils.
- [x] Suppression des copies complètes permanentes, déchargement asynchrone des
  tuiles non utilisées, budgets RAM/disque et état de la pression mémoire.
- [x] Validation édition/Annuler/Rétablir/sauvegarde après éviction, disque plein,
  documents chargés en 8/16 bits et mesures du pic réel de RAM.

Les calques, masques et sources de fréquences utilisent `PixelPlane`. Les outils
à empreintes (pinceaux, retouches, tampons, formes d’historique et correcteurs)
lisent et écrivent des régions directement ; leurs sources composées et les
autres outils peuvent encore matérialiser un plan temporaire. Les formes géométriques
et les dégradés sélectionnés prévisualisent aussi leur seule emprise. Déplacement
et dégradés pleine image gardent leur chemin complet. Les tuiles identiques
gardent leurs versions et leurs mappings lors des écritures régionales. Le budget de
256 Mio par défaut est un objectif de stockage des tuiles, pas un plafond RAM
global. Le composite complet CPU, les sélections et les buffers temporaires
restent à compter séparément. Le pic de RAM est mesuré sur des gestes localisés et sur quatre plans remplis de
24 Mpx en 16 bits. Un volume temporaire de 32 Mio réellement saturé vérifie le
refus d’écriture sans perte des versions ; les disques lents restent à mesurer.

## 2. Composition de la pile sur GPU

Premier chemin disponible : tuiles visibles de 512 px, calques de pixels, texte,
Couleur unie, masques sans contour progressif, visibilité des groupes et écrêtage.
Les 21 modes pris en charge utilisent RGBA8 pour les documents 8 bits et RGBA32F
pour les intermédiaires 16 bits ; les couvertures sont en R8. Les versions
régionales conservent les textures et composites de tuiles inchangées. Le budget
du jeu de travail visible est vérifié avant allocation. Les réglages tonals, les modes
Dissoudre/Mélange maximal/TSL et les contours progressifs utilisent encore le
composite CPU, ainsi que les backends sans formats requis. Ce rendu concerne
l'affichage ; les exports et projets gardent leur référence native CPU.

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

Le répartiteur commun couvre redimensionnement, rotations/miroirs, recadrage de
sélection, profondeur depuis le menu, fusion vers le bas, remplissages/effacement,
inversion des masques, pot de peinture contigu et filtres directs. À partir de quatre millions de pixels
traités, un worker de faible priorité calcule sur un snapshot ; seule la fin valide
applique une étape d’historique. Échap, Annuler ou Annuler/Rétablir rejettent la
demande. Le document peut être fermé ou remplacé sans attendre le worker. Un appel
Qt non interruptible peut terminer son plan courant après l’annulation ; son
résultat n’est jamais appliqué. Les modifications compactes restent synchrones.
L’ouverture RAW, le recadrage interactif, les sélections et la retouche de fréquences
conservent leurs workers spécialisés. Les entrées/sorties de projets et l’export
restent synchrones et constituent une suite distincte.

## 5. Niveaux de détail GPU et budget graphique global

- [x] Pyramide de réduction conservée sur GPU, raccords et phase d’échantillonnage
  vérifiés contre les références actuelles.
- [x] Comptabilité partagée des textures photo/masques/intermédiaires/effets,
  épinglage des ressources en cours d’utilisation et éviction des autres.
- [x] Mesures locales de soumission de trame, tailles de textures et fidélité sur
  Metal/OpenGL ; repli sous petit budget et reconstruction après libération du graphe.
- [ ] Mesures de VRAM physique du pilote et validation Vulkan/Direct3D.

Les tuiles des piles GPU éligibles conservent les réductions 1/2, 1/4, 1/8 et
1/16 sur GPU. Leurs deux passes d’aire partent du composite natif de la tuile,
avec une grille globale au document, y compris pour les dimensions impaires.
Elles ne remplacent pas le calcul natif des filtres ou les exports. Le jeu de
travail inclut toujours la tuile native et ses entrées : si cette réserve ne tient
pas, si le zoom demande un niveau plus petit, ou si la pile est incompatible,
l’affichage réduit conserve son chemin CPU. Il n’y a pas de promesse de réduction
GPU de toutes les piles ni de plafond sur la VRAM totale du pilote.
