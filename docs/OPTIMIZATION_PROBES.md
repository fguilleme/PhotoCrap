# Sondes reproductibles des optimisations

`benchmarks/run_optimization_probes.py` exécute les références de pixels avant de
conserver les mesures. Il n’installe rien et utilise uniquement Python standard.
Les exécutables doivent être construits en Release : `PhotoCrap`,
`photocrap_tests`, `photocrap_tiled_tests`, `photocrap_operation_tests` et
`photocrap_project_write_tests`.

```sh
python3 benchmarks/run_optimization_probes.py --build-dir build
```

Le dossier de résultats temporaire est affiché à la fin. Il contient `results.json`
et un journal par processus. Une exécution en échec reste dans le rapport et fait
échouer le script ; aucun résultat n’est retiré pour améliorer les chiffres.
`--output-dir` permet de choisir un dossier réservé aux résultats.

## Charges et mesures

- Outils, historique et I/O : image native de 2048 × 2048 en 8 puis 16 bits,
  dégradé, déplacement/abandon, 40 petites retouches avec Annuler/Rétablir,
  export PNG par bandes, sauvegarde `.pcr` et réouverture indexée. Les régions
  rouvertes sont comparées exactement. Le rapport distingue temps de chaque
  opération, taille des fichiers, pixels des plans et historique partagé.
- Historique : versions de petites retouches d’une tuile dense dans les trois
  formats natifs, comparaison exacte avant/après déchargement et vidage du cache.
- Pièce : mesure du geste jusqu’au relâchement et jusqu’au résultat observé,
  avec comparaison native après le calcul et relevé des plans conservés.
- Écrivains : PNG/BMP natifs, décodage indépendant, erreurs, écritures courtes et
  annulation. Le temps de cette suite comprend ses vérifications.
- GPU : piles natives 8/16 bits, réglages, masques, modes de fusion éligibles,
  raccords et changements de zoom ; comparaison d’affichage avec le CPU à deux
  niveaux 8 bits maximum. Les pixels de travail et l’export restent natifs CPU.
  Trois modes sont exercés séparément : entrées conservées, entrées réutilisées,
  entrées et intermédiaires partagés. Les flous sont vérifiés séparément.
- Latence GPU : après échauffement, 20 changements du réglage Balance sur une
  image 2049 × 257 en 16 bits. La mesure va de la modification jusqu’au signal
  `frameSwapped` d’une image ayant réellement recomposé la table modifiée, sans le délai de 60 ms utilisé dans les comparaisons de captures.
  Le JSON donne médiane, p95 au rang le plus proche, transferts, passes, API réelle,
  DPR et ressources graphiques. C’est une latence de fenêtre, pas un chronométrage
  isolé du shader. La synchronisation écran et les autres activités influencent
  ces vingt échantillons.
- Aperçus spatiaux : une modification rendue par filtre et profondeur, mesure de
  la présentation et écart RVB échantillonné entre l’aperçu et la référence
  native. La sonde relève aussi les octets de l’aperçu CPU et les ressources
  graphiques comptées par PhotoCrap.

Le temps externe inclut démarrage, vérifications et fermeture. Sur macOS et Linux,
`/usr/bin/time` relève le pic RSS du processus ; le script convertit les unités
Linux en octets. Sans cette sonde, notamment sur Windows, le RSS vaut `null`,
jamais zéro. La RSS inclut Qt, décodeurs, buffers et allocations temporaires ; elle
ne s’identifie ni au budget de tuiles ni au compteur de textures conservées. Les
buffers de transfert et allocations privées du pilote ne sont pas tous exposés par
les compteurs QRhi.

## Tests denses et plateformes

```sh
python3 benchmarks/run_optimization_probes.py --build-dir build --dense
```

`--dense` ajoute le test de quatre plans remplis de 24 Mpx/16 bits avec cache
disque, sauvegarde et réouverture, ainsi que la réduction native 24 Mpx → aperçu.
Il mesure aussi l’écriture et l’ouverture d’un même projet dans deux processus.
Le RSS de l’écriture comprend la préparation des calques dans son processus,
tandis que le chronomètre intégré démarre au début de la sauvegarde.
Prévoir plus de 4 Gio libres pour les fichiers temporaires et suffisamment de RAM
pour les références complètes de ce test. Le budget RAM du document n’est pas un
plafond du processus.

Avec `--dense --scratch-dir /chemin/du/volume`, le test de déchargement crée
son cache sur le volume choisi pour mesurer le comportement de ce disque.

Les contrôles GPU nécessitent une session graphique native. Par défaut, macOS
exécute logiciel, Metal et OpenGL Core ; ailleurs, seul le logiciel est choisi.
On peut demander explicitement une API disponible :

```sh
python3 benchmarks/run_optimization_probes.py --build-dir build --backends vulkan
python3 benchmarks/run_optimization_probes.py --build-dir build --backends d3d11,d3d12
```

Ces commandes sont des parcours de validation à exécuter sur le matériel concerné.
Elles ne signifient pas que Vulkan ou Direct3D ont été validés sur le Mac de
référence. Le rapport conserve l’API réellement utilisée et les erreurs du backend.
La révision Git, l’état modifié des sources suivies, l’architecture et les options
CMake utiles sont enregistrés. `--skip-cpu` permet une nouvelle campagne graphique
après validation du même code CPU ; `--modes shared` cible un mode particulier.

Les seuils de latence ne sont pas imposés en CI : ils dépendraient du matériel.
Les assertions portent sur les pixels, les annulations, la conservation des
versions et les bornes d’allocation, qui doivent rester reproductibles.

## Campagne de référence du 5 novembre 2026

Machine : Apple M2 Pro, 16 Gio, macOS 26.6.2, DPR 2 ; build Release Qt 6.12.0
avec Accelerate. Les contrôles croisés utilisent aussi Qt 6.8.3 et une build sans
Accelerate. Les chiffres suivants décrivent l’état mesuré ; ce n’est pas une
comparaison chronométrée avec une ancienne version.

Temps des opérations sur l’image de 4 Mpx :

| Profondeur | Dégradé | Déplacer/abandonner | 40 retouches | PNG | Sauver PCR | Ouvrir PCR |
|---|---:|---:|---:|---:|---:|---:|
| 8 bits | 10,0 ms | 24,9 ms | 4,4 ms | 101 ms | 154 ms | 38,7 ms |
| 16 bits | 10,8 ms | 28,2 ms | 6,1 ms | 233 ms | 364 ms | 80,6 ms |

Latence des 20 changements réellement rendus dans la petite scène GPU :

| Stockage | Metal médiane / p95 | OpenGL Core médiane / p95 |
|---|---:|---:|
| Entrées conservées | 16,64 / 18,71 ms | 16,53 / 24,61 ms |
| Entrées réutilisées | 25,50 / 36,34 ms | 25,23 / 34,80 ms |
| Intermédiaires partagés | 22,83 / 35,24 ms | 32,31 / 37,89 ms |

Cette petite scène tient dans le budget : le mode conservé y reste préférable.
Les autres modes échangent des transferts supplémentaires contre un jeu de
ressources borné lorsque les piles plus grandes ne tiennent plus. Ils ne doivent
pas être forcés comme accélération universelle. Les références de zoom vérifient
également les ressources réellement retenues contre leur prévision.

Le scénario de quatre plans remplis de 24 Mpx/16 bits passe : 768 000 000 octets
de plans natifs, 1 536 blocs déchargés et zéro octet de tuiles courantes sur le tas
après déchargement. Peinture, Annuler/Rétablir, vidage du cache, sauvegarde et
réouverture sont contrôlés. Le pic RSS du processus atteint environ **1,72 Gio**,
avec décodeurs, mappings et références de test : zéro tuile sur le tas ne signifie
pas zéro RAM résidente.

Validation conservée :

- Qt 6.12 : dix suites ciblées, dont documents, workers, écrivains, tuiles,
  rendu, smoke QML et les trois fermetures, toutes réussies.
- Qt 6.8.3 : cinq suites ciblées et smoke de pile Metal natif réussis.
- CPU portable sans Accelerate : les quatre suites documents/workers/stockage
  réussissent avec leurs références d’arrondi propres.
- Logiciel, Metal et OpenGL Core : les trois modes de stockage et les filtres
  spatiaux passent. Les reports de backend sont vérifiés, sans repli silencieux
  présenté comme une exécution de l’API demandée.
- Vulkan et Direct3D : validation physique encore nécessaire hors de ce Mac.

La campagne graphique correspond à `27e2c52`. Son premier test dense employait à
tort la première entrée du snapshot GPU pour lire le calque inférieur, désormais
exclu du rendu par la photo opaque. L’assertion a été corrigée dans `c9a200f` pour
inspecter séparément le plan masqué puis le plan visible, sans changement de code
produit. Le journal d’échec a été gardé ; le scénario dense et les sept sondes CPU/
logiciel ont ensuite tous réussi. Les deux rapports JSON, nommés
`photocrap-optimization-final-campaign` et `photocrap-optimization-final-cpu-recheck`
dans le dossier temporaire de cette session, conservent cette distinction.

## Mesures séparées des entrées et sorties de projet

Sur le Mac de référence, le projet synthétique de quatre calques de 24 Mpx en
16 bits occupe 18,8 Mo sur disque. Les calques partagent leur dégradé initial et
trois retouches distinctes déclenchent leur encodage séparé.

| Phase | Temps de l’opération | Pic RSS du processus |
|---|---:|---:|
| Sauvegarde | 5,87 s | 265 Mio |
| Ouverture | 1,18 s | 801 Mio |

Après le passage à la lecture Base64 par blocs, une comparaison A/B sur le
même fichier et avec la même version de Qt (6.12) donne les médianes suivantes,
sur trois ouvertures par variante.

| Lecteur | Temps d’ouverture | Pic RSS du processus |
|---|---:|---:|
| Chaîne Base64 chargée par plan | 1,152 s | 803 Mio |
| Base64 décodé par blocs de 16 Kio | 1,149 s | 776 Mio |

Le scénario de déchargement et de réouverture de quatre plans denses passe aussi
sur le SSD externe USB. La campagne sur disque lent demande un volume de ce type.

## Prélèvement sur tous les calques

Sur un document de 24 Mpx avec un flou de boîte visible, la comparaison A/B
Qt 6.12 mesure 80 déplacements de 2 px avec une brosse de 24 px. Deux séries
de dix gestes par variante, après échauffement, donnent les ordres de grandeur
suivants.

| Outil | Premier clic avant / après | Geste complet avant / après |
|---|---:|---:|
| Tampon 16 bits | 0,28 / 0,27 ms | 7,6 / 4,8 ms |
| Doigt 8 bits | 0,07 / 0,07 ms | 1,35 / 1,27 ms |

La fenêtre de réutilisation garde jusqu’à 256 Kio de pixels composés par geste.
Le premier prélèvement reste à la taille demandée. Les empreintes voisines
peuvent réutiliser la fenêtre quand leurs sources sont figées.
