# Aperçus Finder et Quick Look

Les builds macOS embarquent deux extensions natives dans `PhotoCrap.app` :

- **PhotoCrapPreview** affiche les projets `.pcr` avec la barre d’espace dans le Finder.
- **PhotoCrapThumbnail** fournit les miniatures des fichiers.

Elles utilisent les frameworks Apple, sans charger Qt ni lancer l’éditeur.
Le type de document `org.photocrap.project` permet aussi d’ouvrir le projet
dans PhotoCrap depuis le Finder ou le bouton **Ouvrir** de Quick Look.
L’application conserve sa vérification des modifications non enregistrées.

## Utilisation

Installer la nouvelle application dans Applications, puis la lancer une fois.
Dans le Finder, sélectionner un `.pcr` et appuyer sur **Espace**. Pour voir
les miniatures, activer l’aperçu des icônes dans les options de présentation
du Finder. Si macOS désactive une extension, la réactiver dans les réglages
système des extensions Quick Look ; le chemin dépend de la version de macOS.

Les extensions ne sont pas des générateurs `.qlgenerator` à copier dans
`~/Library/QuickLook`. Elles restent intégrées au paquet de l’application.
Leur signature est ad hoc, comme celle des paquets actuels de PhotoCrap ;
la signature Developer ID et la notarisation restent à configurer.

## Gros projets et compatibilité

Chaque nouvel enregistrement ajoute un aperçu JPEG de la composition avant
les données de calques. Son grand côté est limité à **1 280 pixels**, son
JPEG à **512 Kio**, soit environ **683 Kio** au maximum après encodage Base64.
La transparence apparaît sur un damier. Les pixels éditables, leur alpha et
leur précision 8/16 bits restent enregistrés séparément, sans compression JPEG.

Le Finder lit uniquement ce préfixe, avec un tampon de 16 Kio. Il ne charge
ni l’historique, ni les calques complets, ni leurs masques. Le décodeur borne
les dimensions de l’image avant de produire une miniature. Les traitements
d’un même processus d’extension sont sérialisés.

La sauvegarde réutilise le composite complet courant lorsqu’il est disponible.
Sinon, elle produit une composition réduite avec les réglages actuels : elle
peut différer légèrement du rendu intégral pour les filtres spatiaux. La
résolution peut diminuer pour contenir les plans réduits supplémentaires dans
un budget de 64 Mio ; ce budget ne représente pas le RSS total de l’application.
La page d’accueil utilise aussi cet aperçu avant son ancien repli de composition.

Les anciens projets ne contiennent pas de composition intégrée. Les extensions
cherchent alors la première image raster, sans recomposer les calques. Quick
Look l’indique dans le titre et sa miniature porte **PCR\***. Elle peut donc
différer de l’image finale, notamment avec des réglages, masques ou sources IA
recadrées. **Ouvrir et réenregistrer le projet** ajoute l’aperçu final.
Un projet sans image raster lisible n’a pas ce repli.

La lecture de ce repli utilise un fichier temporaire immédiatement dissocié
du système de fichiers, plutôt qu’un gros tampon Base64 en mémoire. Le décodage
est limité à une image de 32 Mpx, 192 Mio de données PNG/JPEG et 256 Mio de texte
Base64 ; le parcours et le décodage Base64 ont chacun un délai de cinq secondes.
Ces bornes peuvent empêcher l’aperçu d’un ancien projet atypique ou très lent
à lire. Aucun fichier utilisateur n’est modifié par les extensions.

Le champ `preview` est facultatif : le numéro minimal du format reste **v9 à
v13** selon le contenu du projet, et les anciens lecteurs ignorent cet aperçu.

## Développement et vérification

La compilation CMake macOS construit et signe les deux `.appex`, puis les
copie dans `PhotoCrap.app/Contents/PlugIns`. L’installation et la vérification
du paquet contrôlent leur présence et la signature de l’application. Le
sandbox des extensions autorise la lecture des fichiers fournis par macOS,
sans accès réseau.

Tests automatisés : `project_preview`, `application_file_open` et
`macos_quicklook_reader`. Le dernier contrôle notamment un fichier creux de
200 Mio dont seul le préfixe de 16 Kio est nécessaire, les anciens PNG et les
entrées invalides. Les tests de sauvegarde vérifient aussi la composition et
la conservation exacte des données natives 8/16 bits.

Le binaire de test macOS accepte des contrôles manuels :

```sh
build/photocrap_quicklook_tests --fixture /tmp/quicklook-check.pcr
build/photocrap_quicklook_tests --finder /tmp/quicklook-check.pcr
build/photocrap_quicklook_tests /chemin/ancien-projet.pcr
```

Le contrôle `--finder` nécessite que macOS ait enregistré l’application et ses
extensions ; il demande une vraie miniature, sans accepter une simple icône
de fichier. Il n’est pas imposé aux runners CI.

Références Apple : [miniatures personnalisées](https://developer.apple.com/documentation/quicklookthumbnailing/providing-thumbnails-of-your-custom-file-types),
[Quick Look UI](https://developer.apple.com/documentation/quicklookui/),
[QLPreviewingController](https://developer.apple.com/documentation/quicklookui/qlpreviewingcontroller?language=objc).
