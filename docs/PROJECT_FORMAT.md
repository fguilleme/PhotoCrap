# Format des projets `.pcr`

## Conteneur binaire v14 — non publié

Les nouvelles sauvegardes utilisent un conteneur binaire, sans chaînes Base64.
Les anciens fichiers JSON v1–v13 restent lisibles ; les réenregistrer les convertit
au nouveau conteneur. Les anciennes versions de PhotoCrap ne peuvent pas lire ces
nouvelles sauvegardes. La migration conserve le chemin et utilise un remplacement
atomique ; pour garder une copie compatible avec un ancien éditeur, conserver
l’ancien fichier séparément avant de l’enregistrer.

Les pixels éditables sont compressés **sans perte** avec **Zstandard 1.5.7**, niveau
3, après une décorrélation réversible rouge−vert / bleu−vert, un prédicteur
Paeth et une séparation des octets des canaux. Les échantillons prémultipliés sont conservés exactement, y compris les
faibles alphas et les valeurs 16 bits ; aucun aller-retour par le PNG ni réduction
à 8 bits ne sont nécessaires. Le niveau choisi privilégie le compromis taille /
vitesse, et ne garantit pas une taille inférieure au PNG sur toute image.

- Tuiles indépendantes de **256×256 pixels**, avec bords partiels.
- RGBA prémultiplié 8 ou 16 bits par canal ; masques en niveaux de gris 8 bits.
- Ordre RGBA canonique ; valeurs 16 bits en little-endian.
- Tuiles absentes représentées par la couleur de fond native du plan.
- Plans uniformes stockés par une couleur et leurs dimensions logiques.
- Un même plan partagé est référencé une fois ; les tuiles identiques entre plans
  différents sont dédupliquées par SHA-256, dimensions et format.
- Une tuile peu compressible est conservée en octets natifs si Zstandard
  l’agrandit. Son empreinte SHA-256 est toujours vérifiée à la lecture.
- Les sorties IA non retouchées conservent leur fichier d’origine PNG/JPEG/WebP
  comme bloc binaire, sans recompression ni Base64. Les retouches utilisent les
  tuiles natives ci-dessus.

L’aperçu Finder est un **JPEG d’affichage**, séparé des données éditables. Sa
compression avec perte n’affecte pas les pixels du document. Il est disponible
immédiatement après l’en-tête, sans charger l’index ni le codec Zstandard.

## Disposition et limites

Tous les nombres de l’en-tête sont des entiers non signés little-endian.

| Position | Taille | Contenu |
|---|---:|---|
| 0 | 8 | Signature `PCRBIN\r\n` |
| 8 | 8 | Version du conteneur : 14 |
| 16 | 8 | Taille de l’aperçu JPEG, 0 à 512 Kio |
| 24 | 8 | Position de l’index |
| 32 | 8 | Taille compressée de l’index |
| 40 | 8 | Taille décompressée de l’index |
| 48 | 8 | Taille exacte du fichier |
| 56 | 8 | Réservé, zéro |
| 64 | variable | Aperçu JPEG, puis blocs de données |
| position de l’index | variable | Index CBOR compressé avec Zstandard |

L’index CBOR contient `document`, `planes` et `blocks`. Les métadonnées du document
conservent leur schéma fonctionnel v9–v13 : calques, réglages, masques, texte,
formes, dégradés, tracés, tranches et provenance IA. Ces numéros de schéma ne sont
pas la version du conteneur. Les anciens noms internes `png`, `mask`, `source`
référencent désormais des identifiants de plans entiers ; ils ne contiennent
aucune image encodée en texte. `-1` représente l’absence d’un plan.

Un descripteur de plan est `[largeur, hauteur, octetsParPixel, fond, tuiles, métriques]`.
Les métriques conservent `[devicePixelRatio, dotsPerMeterX, dotsPerMeterY]`.
Le fond est une chaîne d’octets CBOR native. Une liste de tuiles vide représente
un plan uniforme ; sinon les identifiants sont ordonnés par lignes, avec `-1`
pour une tuile égale au fond. Un plan contenant un fichier IA original utilise
`[0, 0, 0, octetsVides, identifiantBloc, tableauVide]`.

Un bloc est décrit par `[position, tailleStockée, tailleNative, codec, prédicteur,
largeur, hauteur, octetsParPixel, sha256]`. Le codec vaut 1 pour Zstandard, 0 pour les octets bruts. Le prédicteur vaut 2
pour décorrélation couleur + Paeth + séparation des octets, 1 pour delta horizontal
avec séparation des octets, 0 pour les octets bruts. Les données originales
IA utilisent des dimensions et une profondeur nulles. Les blocs uniques sont
contigus, dans leur ordre d’écriture, avant l’index ; aucune entrée ne peut
pointer hors de cette zone ou chevaucher un autre bloc.

Limites du lecteur et de l’écriture :

- fichier binaire : **8 Gio** ; anciens fichiers JSON : **256 Mio** ;
- index compressé et décompressé : **32 Mio** chacun ;
- une tuile native : **512 Kio** maximum ; fichier IA original : **100 Mio** ;
- 512 calques, 32 Mpx et budget logique de plans de 2 Gio inchangés ;
- au plus 1 536 plans et 262 144 blocs uniques ;
- fenêtre de décompression Zstandard bornée à 8 Mio.

Les dimensions, formats, références et réservations mémoire sont validés avant
publication du document. Les checksums Zstandard et les SHA-256 des blocs détectent
les erreurs de données. Un chargement échoué ou annulé conserve le document ouvert.
Les lectures se font par fragments de 16 Kio et la compression / décompression
par tuile, sans tampon contenant toutes les images du projet. L’index et les
empreintes de déduplication restent en mémoire, dans les limites indiquées.

L’écriture utilise `QSaveFile`, avec mise à jour finale de l’en-tête. Annuler ou
échouer avant publication conserve le fichier précédent. Les grandes sauvegardes
restent exécutées en arrière-plan.

## Références

[Zstandard : dépôt officiel](https://github.com/facebook/zstd),
[API officielle](https://facebook.github.io/zstd/zstd_manual.html),
[CBOR / RFC 8949](https://www.rfc-editor.org/rfc/rfc8949.html).
Zstandard est embarqué statiquement avec sa licence BSD ; aucune installation
de codec système n’est nécessaire.
