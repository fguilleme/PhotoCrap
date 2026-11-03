# Migration des gros documents et du rendu

Travail demandé le 2 novembre 2026 : réaliser les cinq chantiers ci-dessous dans
l’ordre, avec commits distincts et validation avant chaque livraison. Cette liste
décrit le travail restant ; elle ne constitue pas une déclaration de fonctionnalités
déjà disponibles.

## 1. Plans éditables et historique par tuiles

- [x] Import exact des plans existants dans `TiledImage`, remplacement atomique et
  réutilisation des versions des tuiles inchangées, y compris après déchargement disque.
- [x] Stockage des plans des calques/masques et de leurs versions d’historique dans
  ces plans ; lectures régionales par la composition et les outils.
- [x] Suppression des copies complètes permanentes, déchargement asynchrone des
  tuiles non utilisées, budgets RAM/disque et état de la pression mémoire.
- [ ] Validation édition/Annuler/Rétablir/sauvegarde après éviction, disque plein,
  documents chargés en 8/16 bits et mesures du pic réel de RAM.

Les calques, masques et sources de fréquences utilisent `PixelPlane`. Pinceau,
Crayon et Gomme écrivent des régions directement ; les autres outils peuvent
encore matérialiser un plan temporaire pendant leur opération. Le budget de
256 Mio par défaut est un objectif de stockage des tuiles, pas un plafond RAM
global. Le composite complet CPU, les sélections et les buffers temporaires
restent à compter séparément. Les essais sur disque réellement saturé et les
mesures du pic de RAM global restent ouverts.

## 2. Composition de la pile sur GPU

Premier chemin disponible : tuiles visibles de 512 px, calques de pixels, texte,
Couleur unie, masques sans contour progressif, visibilité des groupes et écrêtage.
Les 21 modes pris en charge utilisent RGBA8 pour les documents 8 bits et RGBA32F
pour les intermédiaires 16 bits ; les couvertures sont en R8. Les versions
régionales conservent les textures et composites de tuiles inchangées. Le budget
du jeu de travail visible est vérifié avant allocation. Les réglages, les modes
Dissoudre/Mélange maximal/TSL et les contours progressifs utilisent encore le
composite CPU, ainsi que les backends sans formats requis. Ce rendu concerne
l'affichage ; les exports et projets gardent leur référence native CPU.

- [ ] Composition des calques visibles par tuiles, masques et modes de fusion,
  invalidation régionale et conservation des intermédiaires utiles.
- [ ] Choix explicite des formats/précisions intermédiaires, références CPU et
  replis pour les combinaisons non prises en charge et le backend logiciel.
- [ ] Vérification des groupes, écrêtages, transparences, ordre des effets,
  redimensionnement, perte du contexte et limites de texture.

## 3. Filtres spatiaux sur GPU

- [ ] Flous séparables et voisinages avec halos de tuiles, puis netteté et effets
  dérivés ; précision native préservée lorsque ce chemin produit le rendu final.
- [ ] Annulation et rejet des rendus périmés ; comparaison CPU aux rayons limites
  et aux frontières des tuiles en 8/16 bits.

## 4. Généralisation de l’asynchrone

- [ ] Inventaire des opérations synchrones restantes et snapshots immuables.
- [ ] Transformations, opérations complètes de calques et filtres directs : progrès,
  annulation, validation atomique et unique étape d’historique.
- [ ] Tests de fermeture/changement de document pendant une opération, erreurs
  de ressources et priorité aux interactions/aperçus récents.

## 5. Niveaux de détail GPU et budget graphique global

- [ ] Pyramide de réduction conservée sur GPU, raccords et phase d’échantillonnage
  vérifiés contre les références actuelles.
- [ ] Comptabilité partagée des textures photo/masques/intermédiaires/effets,
  épinglage des ressources en cours d’utilisation et éviction des autres.
- [ ] Mesures de latence, RAM/VRAM et fidélité par backend ; comportement correct
  sous petit budget et reconstruction après perte de ressources.
