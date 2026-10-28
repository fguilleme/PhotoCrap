# Calques, mémoire et cache de travail disque

Audit du 28 octobre 2026, après v0.14. Les optimisations préparatoires sont committées
séparément dans `862e505`. Une première étape du cache disque est implémentée pour les
rendus de fréquences. Le déchargement des pixels de calques, masques et historique
reste à réaliser ; l’architecture par tuiles ci-dessous décrit cette suite.

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
- La passe en cours ajoute une pile de **64 calques réellement remplis**, en 8 et
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
smoke test QML couvre la page de réglages. La réserve insuffisante est simulée ; les essais
sur un disque réellement saturé et les mesures sur disques lents restent à compléter.

Ce premier usage conserve des rendus recalculables. Il ne décharge pas encore les plans
éditables ni l’historique et ne relève aucune limite du document. Le même stockage de
blocs pourra recevoir des tuiles quand l’accès aux pixels aura été adapté.

Références des contrats Qt utilisés : [QImage et buffers externes](https://doc.qt.io/qt-6/qimage.html),
[mapping de fichiers](https://doc.qt.io/qt-6/qfiledevice.html#map).

## Étape suivante : stockage de travail par tuiles

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
