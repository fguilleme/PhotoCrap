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
Prévoir plus de 4 Gio libres pour les fichiers temporaires et suffisamment de RAM
pour les références complètes de ce test. Le budget RAM du document n’est pas un
plafond du processus.

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
