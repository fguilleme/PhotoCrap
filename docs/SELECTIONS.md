# Sélections : référence Adobe et état PhotoCrap

Audit du 28 octobre 2026, pour **v0.14**. La capture du menu sert à vérifier la présentation ; les comportements et options ci-dessous proviennent de la documentation Photoshop pour ordinateur. PhotoCrap utilise ses propres algorithmes : des commandes portant le même nom ne garantissent pas un résultat pixel pour pixel identique.

## Trois opérations différentes

Une sélection délimite les pixels auxquels une action s’applique. Déplacer son **contour** laisse les pixels immobiles ; déplacer son **contenu** modifie le calque. Masquer les fourmis ne désélectionne pas. Sélectionner des **calques** dans la liste est encore une autre opération.

Références : [déplacement de la sélection ou de son contour](https://helpx.adobe.com/photoshop/desktop/make-selections/refine-modify-selections/move-selection-or-selection-border.html), [sélection de calques](https://helpx.adobe.com/photoshop/desktop/create-manage-layers/transform-manipulate-layers/select-layers.html).

## Commandes disponibles

Commande/Ctrl désigne Commande sur Mac et Ctrl sur Windows/Linux. Les raccourcis sont personnalisables dans Réglages → Raccourcis clavier. Les nouvelles commandes sans raccourci par défaut peuvent y recevoir une combinaison.

| Commande | Comportement et paramètres | PhotoCrap |
|---|---|---|
| Tout sélectionner | Toute la surface du document ; Commande/Ctrl+A | Disponible |
| Désélectionner | Retire la sélection active ; Commande/Ctrl+D | Disponible ; annulation et resélection possibles |
| Resélectionner | Rétablit le dernier contour retiré ; Commande/Ctrl+Maj+D | Disponible tant qu’une sélection précédente existe et qu’aucune n’est active |
| Intervertir | Sélectionne le complément à l’intérieur du document ; Commande/Ctrl+Maj+I | Disponible ; trous et zones disjointes conservés |
| Plage de couleurs | Couleurs prélevées sur le canevas ou l’aperçu, tolérance, pipettes +/−, portée localisée, zones contiguës, inversion ; aperçus zoomables image et masque | Disponible ; voir les limites ci-dessous |
| Modifier → Contour progressif | Transition graduelle réelle ; rayon **1–1 000 px**, Maj+F6 | Disponible ; calcul asynchrone et couverture indépendante de la précision 8/16 bits de l’image |
| Modifier → Frange | Bande centrée sur la limite ; largeur **1–200 px** | Disponible à bords francs ; largeur impaire arrondie vers l’intérieur |
| Modifier → Lisser | Élimine les irrégularités suivant la majorité du voisinage ; rayon **1–100 px** | Disponible ; voisinage carré, égalité conservant le pixel initial |
| Modifier → Dilater | Élargit la sélection ; distance **1–100 px** | Disponible ; distance euclidienne, bornée au document |
| Modifier → Contracter | Réduit la sélection ; distance **1–100 px** | Disponible ; le bord extérieur du document n’est pas traité comme un trou |
| Tous les calques | Sélection multiple des calques ; Commande/Ctrl+Option/Alt+A | Disponible |
| Calques similaires | Sélectionne les calques de même nature que le calque actif | Disponible : pixels, réglages, remplissages, groupes… |
| Affichage → Afficher → Contour de la sélection | Cache/affiche les fourmis en conservant la sélection | Disponible ; toute nouvelle sélection réaffiche son contour |

Sources : [dilater et contracter](https://helpx.adobe.com/photoshop/desktop/make-selections/refine-modify-selections/expand-or-contract-selection.html), [frange](https://helpx.adobe.com/photoshop/desktop/make-selections/refine-modify-selections/create-selection-around-selection-border.html), [lissage et pixels isolés](https://helpx.adobe.com/au/photoshop/desktop/make-selections/refine-modify-selections/clean-up-stray-pixels-in-color-based-selection.html). Adobe décrit les paramètres et le principe ; le voisinage exact, les égalités et les arrondis décrits ici sont les choix de PhotoCrap. Sur une sélection progressive, Dilater/Contracter/Frange utilisent des extrema dans un voisinage carré et Lisser une moyenne locale, afin de conserver les couvertures partielles.

Les commandes Modifier ouvrent une boîte de paramètres, puis calculent **en arrière-plan**. Annuler ou Échap pendant le calcul abandonne le résultat. Une nouvelle sélection, une édition ou un autre document invalide les travaux obsolètes. Le traitement porte sur la zone sélectionnée et le voisinage nécessaire, sans recalculer les couleurs de l’image. Les sélections et leur déplacement sont annulables ; ces états ne rendent pas un projet sauvegardé « modifié ». Leur stockage supplémentaire entre dans le budget de l’historique.

## Plage de couleurs et contour progressif

Dans **Sélection → Plage de couleurs**, cliquer sur l’aperçu ou directement sur le canevas prélève une couleur à sa résolution native, sans fermer le dialogue. Maj ajoute, Option/Alt retire ; les trois boutons proposent aussi ces modes. Jusqu’à 64 échantillons peuvent être retenus. La tolérance va de 0 à 200. Le mode localisé limite l’influence autour des échantillons ; Portée est exprimée en pourcentage de la diagonale. Le masque montre les pixels sélectionnés en blanc, les exclusions en noir et les couvertures partielles en gris. Les familles Rouge/Jaune/Vert/Cyan/Bleu/Magenta et Hautes lumières/Tons moyens/Ombres sont proposées comme points de départ. OK remplace la sélection en une étape ; Annuler conserve l’ancienne sélection et l’historique. Une absence de correspondance conserve la sélection précédente et affiche un message.

L’encodage couleur et les seuils sont propres à PhotoCrap : distance maximale entre composantes RVB à leur précision native, décroissance linéaire avec la tolérance, étendue locale radiale. Les familles utilisent la teinte et la saturation, les tons la luminance. Les modes Tons chair/Détection des visages, Hors gamut CMJN, préréglages de plage enregistrables, réglages tonals détaillés et les autres modes d’aperçu Adobe restent à ajouter. Le dialogue peut être déplacé par son en-tête pour dégager l’image. Les clics ailleurs ne le ferment pas ; OK valide et Annuler ou Échap abandonne. Les autres éditions du document restent suspendues pendant ce prélèvement. Source : [Plage de couleurs Adobe](https://helpx.adobe.com/photoshop/desktop/make-selections/freehand-selections/select-a-color-range-in-photoshop.html).

Les deux aperçus partagent le zoom et le déplacement : boutons −/+, 100 % et Adapter, molette pour zoomer autour du pointeur, glisser pour déplacer. Le pourcentage correspond à la résolution native ; un zoom rapproché lit la portion visible des pixels d’origine plutôt que d’agrandir le proxy. Sur le canevas principal, les raccourcis de zoom restent disponibles, Commande/Ctrl+molette zoome autour du pointeur et la molette défile. Un glisser déplace la vue sans peindre.

**Zones contiguës** est une option supplémentaire de PhotoCrap, indépendante des groupes localisés : désactivée, elle recherche les correspondances dans toute l’image ; activée, elle conserve seulement les composantes reliées par leurs bords à un prélèvement positif. Maj-clic peut donc ajouter un autre îlot. Les prélèvements négatifs soustraient uniquement leur propre composante lorsque l’option est active. Une connexion diagonale seule ne relie pas deux zones. Les couvertures progressives et les pixels transparents sont respectés ; l’inversion intervient après cette restriction. Le parcours est asynchrone et annulable comme le calcul de couleur.

**Contour progressif** crée un adoucissement par approximation gaussienne à trois passes séparables ; les calculs intermédiaires sont en 16 bits pour ne pas faire disparaître les petites couvertures par arrondis successifs. Le rayon pilote cet adoucissement, sans garantie de correspondance pixel à pixel avec Photoshop. Une petite sélection peut rester active sans aucune fourmi si aucun pixel ne dépasse 50 %. La douceur est utilisée par le pinceau, la gomme, les remplissages, les filtres directs, la copie, les déplacements et les masques créés depuis la sélection. Les opérations conservent les canaux 16 bits de l’image lorsqu’ils sont pris en charge par l’outil. L’historique, Resélectionner et les combinaisons avec les outils géométriques conservent la couverture. La sélection reste temporaire : les couches alpha nommées et leur sauvegarde sont encore NYI. Source : [Contour progressif Adobe](https://helpx.adobe.com/photoshop/desktop/make-selections/refine-modify-selections/define-feathered-edges.html).

## Fonctions recensées mais NYI

Ces commandes sont visibles et désactivées dans le menu et répertoriées dans les raccourcis. Leur activation attend une implémentation complète ; aucune commande inactive ne capture son raccourci.

| Fonction | Comportement Adobe à prendre en charge | Travail restant |
|---|---|---|
| Zone de mise au point | Plage de netteté automatique ou manuelle, ajout/retrait au pinceau, bruit, adoucissement ; sortie en sélection, masque, calque ou document | Analyse de netteté, sélection progressive et sorties |
| Sujet | Détection du ou des sujets, puis affinage | Modèle de segmentation et inférence asynchrone ; un seuil de couleur ne suffit pas |
| Ciel | Détection du ciel, raffinage autour des objets et du feuillage | Segmentation et affinage ; éviter une simple sélection des pixels bleus |
| Sélectionner et masquer | Modes d’affichage, transparence, détection de contours, rayon, cheveux, lissage, contour progressif, contraste, décalage du contour, décontamination et sorties | Espace d’affinage et couverture progressive ; détails fins à valider |
| Étendre | Ajoute les couleurs proches **contiguës** selon la tolérance de la Baguette magique | Échantillons depuis toute la sélection, connexité et tolérance native |
| Généraliser | Même recherche de couleurs dans **toute l’image**, sans contrainte de contiguïté | Recherche globale ; ne pas confondre avec une dilatation géométrique |
| Transformer la sélection | Poignées, proportions, pivot, position, dimensions, angle, déformation/perspective ; Retour valide, Échap abandonne ; contenu immobile | Transformation du contour et interface dédiées ; le déplacement simple existe déjà |
| Mode Masque | Q bascule l’édition d’une couverture temporaire : noir retire, blanc ajoute, gris sélectionne partiellement ; surimpression réglable | Canal temporaire, peinture progressive et retour à la sélection ; distinct d’un masque de calque |
| Mémoriser / Récupérer la sélection | Couches alpha nommées, dimensions compatibles, remplacement/ajout/soustraction/intersection et inversion | Couches alpha persistantes et nouveau format de sauvegarde ; Resélectionner n’en tient pas lieu |
| Désélectionner les calques | Aucun calque actif, sans désélectionner les pixels | Le moteur impose encore un calque actif ; adapter les outils et actions avant activation |
| Rechercher les calques | Recherche et filtrage dans le panneau Calques | Filtrage du modèle et champ de recherche |
| Isoler les calques | Filtre du panneau sur les calques sélectionnés | Filtrage du modèle ; distinct de l’Alt-clic sur l’œil qui change la visibilité |

Références détaillées : [Plage de couleurs](https://helpx.adobe.com/photoshop/desktop/make-selections/freehand-selections/select-a-color-range-in-photoshop.html), [mise au point](https://helpx.adobe.com/photoshop/desktop/make-selections/refine-modify-selections/select-areas-and-pixels-in-focus.html), [Sujet](https://helpx.adobe.com/photoshop/desktop/quick-actions/select-subject-in-images.html), [Ciel](https://helpx.adobe.com/photoshop/desktop/quick-actions/select-skies-in-images.html), [affinage et masque](https://helpx.adobe.com/photoshop/desktop/make-selections/refine-modify-selections/refine-your-selection-and-mask.html), [adoucissement des bords](https://helpx.adobe.com/photoshop/desktop/make-selections/refine-modify-selections/refine-and-soften-selection-edges.html), [transformations](https://helpx.adobe.com/photoshop/desktop/crop-resize-transform/resize-adjust-resolution/transform-images-shapes-and-paths.html), [Mode Masque](https://helpx.adobe.com/photoshop/desktop/create-masks/layer-masks/create-a-temporary-quick-mask.html), [mémorisation et récupération](https://helpx.adobe.com/photoshop/desktop/create-masks/layer-masks/save-and-load-selections.html).

## Au-delà du menu

- Les outils géométriques, les lassos libre/polygonal, la Baguette magique et la Sélection rapide sont disponibles. Maj ajoute, Option/Alt soustrait et Maj+Option/Alt intersecte. Les gestes sont décrits dans [TOOL_INTERACTIONS.md](TOOL_INTERACTIONS.md#sélections). La sélection d’objet, le pinceau de sélection et le lasso magnétique restent NYI.
- La Plume produit un contour géométrique et peut le convertir en sélection. La conversion directe avec paramètres de contour progressif ou anticrénelage fractionnaire reste NYI ; la commande Contour progressif peut être appliquée ensuite.
- Charger la transparence d’un calque, un masque ou une couche RVB comme sélection, puis combiner les couvertures avec les modificateurs de vignette, reste NYI. Référence : [charger depuis le contenu d’un calque ou un masque](https://helpx.adobe.com/photoshop/desktop/make-selections/get-started-selections/select-non-transparent-or-unmasked-areas.html).
- Les fourmis ne représentent qu’un contour : une couverture inférieure à 50 % peut exister sans contour visible dans Photoshop. PhotoCrap conserve maintenant une couverture **0–255 par pixel** pour les sélections progressives ; l’image de travail reste 8/16 bits par canal. Les fourmis suivent le seuil de 50 %, tandis que les traitements utilisent aussi les couvertures inférieures. L’anticrénelage fractionnaire des outils géométriques reste une étape distincte.
- **Extras**, généralement Commande/Ctrl+H, concerne aussi d’autres repères. Il reste NYI : la nouvelle commande Contour de la sélection ne lui est pas substituée et n’a pas de raccourci imposé.
- Les opérations sur les pixels, les masques de calque, les tracés et les couches alpha doivent conserver des contrats distincts. Les sélections temporaires des outils Pièce et Déplacement basé sur le contenu restent intégrées à leur unique opération de retouche.

## Validation locale

La suite `selection` compare les transformations à une référence exhaustive par pixel sur des zones trouées, disjointes et touchant le document. Elle couvre l’annulation, les résultats obsolètes, les états propre/modifié d’un projet sauvegardé, l’historique de la Plume et le stockage des contours. Le smoke test QML vérifie les commandes menu fermé, les dialogues, le rendu des limites et l’annulation par Échap. Les suites existantes de document, Plume et fermeture complètent ces contrôles.
