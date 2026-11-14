# Création de calques avec une IA

**Fichier → Créer avec l’IA…** et **Calques → Créer avec l’IA…** ouvrent le même
éditeur. Choisir une configuration de fournisseur, la composition visible entière
ou la sélection, puis un prompt enregistré ou un texte libre si le modèle le demande. **Générer** lance
l’envoi ; ouvrir le dialogue ou changer de preset n’envoie rien.

## Fournisseurs et prompts

Dans **Réglages → IA**, deux onglets permettent d’enregistrer plusieurs
configurations de fournisseurs et une bibliothèque de prompts réutilisables.
La liste des fournisseurs reste visible à gauche. **+ → OpenAI / fal.ai / Qwen Edit 2.1 · Gradio / ComfyUI / API compatible** crée immédiatement un profil. Compléter sa configuration puis utiliser **Enregistrer**, toujours visible en bas à droite. Un message confirme la sauvegarde ou indique précisément l’erreur. On peut ainsi
conserver plusieurs modèles OpenAI, endpoints fal.ai ou serveurs locaux.

| Type | Configuration | Protocole |
| --- | --- | --- |
| OpenAI · GPT Image | Adresse terminant par `/v1`, modèle accessible au compte, clé API | `POST /images/edits`, image PNG et masque multipart, résultat encodé ou URL |
| fal.ai | Serveur de file, chemin du modèle d’édition | Envoi JSON puis suivi de la file et téléchargement du résultat |
| Qwen Edit 2.1 · Gradio | Adresse du serveur, nom d’API, utilisateur et mot de passe HTTP Basic | JSON nommé v2 puis flux SSE, image base64 et graine réelle |
| ComfyUI | Adresse du serveur, checkpoint installé, workflow au format API | Upload des images, soumission du workflow, suivi de son historique, récupération de l’image |
| API compatible OpenAI | Serveur implémentant réellement l’édition d’images OpenAI | Même protocole multipart que GPT Image ; une API de chat seule ne suffit pas |

Les configurations initiales utilisent `gpt-image-2.5-sunburst`,
`fal-ai/flux-2/edit` et un serveur ComfyUI à `http://127.0.0.1:8188`.
Les noms de modèles restent modifiables selon les droits du compte ou le serveur.
PhotoCrap n’installe ni modèle ni serveur local.

La clé API saisie puis enregistrée est conservée par **QtKeychain** dans le
trousseau macOS, le gestionnaire d’identifiants Windows ou le service de secrets
Linux (GNOME Keyring/libsecret ou KWallet). Elle est retrouvée lors du premier
clic sur **Générer** pour ce fournisseur, puis réutilisée en mémoire pendant la
session, sans apparaître dans les champs, les réglages en clair ou les projets.
Le démarrage de PhotoCrap et l’ouverture des réglages ne sollicitent pas le trousseau.
L’interface peut indiquer **Clé enregistrée** à partir d’un indicateur non secret ;
les anciens profils sont vérifiés au premier lancement de génération. Laisser
le champ vide conserve la clé existante.
Les variables d’environnement (`OPENAI_API_KEY`, `FAL_KEY` par défaut) restent
une alternative, visible dans le statut du fournisseur. Une clé enregistrée pour le profil
est prioritaire sur sa variable d’environnement, y compris après un redémarrage.
Le champ indique lorsqu’une nouvelle clé attend encore son enregistrement ; pendant
la sauvegarde, le formulaire est verrouillé jusqu’au résultat du trousseau.

Les profils **fal.ai** utilisant le même serveur peuvent réutiliser une clé déjà
enregistrée : par exemple, Real-ESRGAN retrouve celle du profil FLUX. L’interface
indique **Clé partagée** et le nom du profil source. Le même mécanisme existe entre
profils OpenAI du même endpoint. Une clé propre au profil reste prioritaire, puis
sa variable d’environnement. S’il existe plusieurs profils porteurs d’une clé pour
le même service, renseigner explicitement celle du profil choisi pour éviter toute
ambiguïté de compte. Aucun partage n’a lieu entre OpenAI et fal.ai, entre serveurs
différents, ou pour les serveurs ComfyUI/API compatibles. La clé partagée reste liée
au profil source ; supprimer sa clé la retire donc aussi aux profils qui la réutilisent.

Sur macOS, les builds actuelles sont signées ad hoc : leur identité est liée
au hash du binaire et change après recompilation. Le système peut donc redemander
une autorisation au premier usage d’une clé avec une nouvelle build. Une identité
de signature stable reste nécessaire pour conserver la confiance entre versions.
Voir le [guide Apple sur les exigences de signature](https://developer.apple.com/library/archive/documentation/Security/Conceptual/CodeSigningGuide/RequirementLang/RequirementLang.html).
Si macOS renvoie une erreur d’authentification lors de la lecture, PhotoCrap
réessaie cette lecture une fois. Une annulation explicite dans le trousseau
met fin à la demande.

La suppression du fournisseur ou la modification de son adresse/protocole supprime
sa clé enregistrée. Une modification purement équivalente de l’adresse (barre finale,
majuscules de l’hôte ou port HTTPS 443) conserve la clé. Les identifiants sont liés à l’UUID du profil et à son endpoint.
Les téléchargements d’images externes ne reçoivent pas la clé du fournisseur.
**Options avancées → Supprimer la clé enregistrée** permet de l’effacer séparément.
Si le trousseau est indisponible ou verrouillé, l’interface affiche l’erreur ; aucun
repli en stockage clair n’est autorisé. Sous Linux, installer `libsecret-1` et
activer le service de secrets de la session. Le build requiert `libsecret-1-dev`.

Depuis la génération, **Gérer les fournisseurs…** ouvre cette page. Après
l’enregistrement, **Fermer** ramène à la génération, avec le profil sélectionné et
le prompt conservé. Aucun envoi n’a lieu pendant la configuration.

Les options avancées acceptent un objet JSON pour les paramètres du modèle.
Les champs d’image et les champs de génération requis restent gérés par PhotoCrap ; les autres paramètres du modèle sont transmis depuis cet objet.
Pour fal.ai, le nom du champ image et son caractère liste/valeur sont configurables
pour adapter les modèles d’édition. Les options ne rendent pas interchangeables
les schémas des différents modèles.

## Qwen Edit 2.1 avec API Gradio v2

**Réglages → IA → + → Qwen Edit 2.1 · Gradio** crée un profil. Remplacer l’adresse
locale d’exemple par l’adresse de votre serveur, conserver **Nom de l’API :
`edit_image`**, puis choisir **HTTP Basic** et saisir les mêmes identifiants que
l’interface du serveur. Utiliser **Enregistrer** : le mot de passe passe dans le
trousseau sécurisé, jamais dans le JSON de requête, les réglages en clair ou le
projet. Un changement d’utilisateur ou de mode d’authentification retire le secret
précédent ; il faut alors le saisir de nouveau. L’option sans authentification
existe pour les serveurs qui n’en demandent pas.

Le profil vise l’API **nommée v2** du serveur Qwen fourni, et non n’importe quelle
application Gradio acceptant des paramètres positionnels `{"data":[...]}` :

1. `POST /gradio_api/call/v2/edit_image` envoie les paramètres JSON nommés et reçoit
   `event_id`.
2. `GET /gradio_api/call/edit_image/{event_id}` lit les événements SSE. Les
   heartbeats et résultats intermédiaires sont ignorés ; seul `complete` fournit
   l’image finale. Une erreur ou une fermeture prématurée est affichée.

Les appels Qwen utilisent des connexions HTTP/1.1 distinctes, sans réutiliser la
connexion d’un ancien flux SSE. Le délai de transfert applicatif du flux SSE est
désactivé ; le délai global reste de 15 minutes. Une coupure de lecture
ou une indisponibilité temporaire déclenche au plus trois reprises du **même
`event_id`**, sans renvoyer l’image ni créer une seconde tâche. La récupération
dépend de la conservation de cette tâche par le serveur ; un refus
d’authentification, une tâche perdue ou une erreur de génération ne sont pas
relancés automatiquement. Une annulation empêche également les reprises différées.
Si la connexion initiale échoue avant l’envoi de la requête, PhotoCrap
réessaie une fois. Si la connexion est coupée après le début de l’envoi
et que la réception reste incertaine, PhotoCrap le signale.

Les deux requêtes utilisent HTTP Basic lorsque ce mode est sélectionné. Exemple
de corps construit automatiquement pour une image ou la sélection active :

```json
{
  "images_b64_json": "[\"data:image/png;base64,...\"]",
  "prompt": "Transforme cette photo en dessin animé",
  "lora_adapter": "Photo-to-Anime",
  "seed": 42,
  "randomize_seed": false,
  "guidance_scale": 1.0,
  "steps": 4
}
```

`images_b64_json` est une **chaîne JSON contenant une liste d’une image PNG**.
PhotoCrap n’envoie pas les autres documents ouverts. Il décode le champ final
`image` (data URI PNG/JPEG/WebP) et conserve la graine effectivement renvoyée dans
les informations du résultat. Les sorties JSON Gradio enveloppées dans un tableau
sont également acceptées. [Protocole SSE Gradio](https://gradio.app/4.44.1/guides/querying-gradio-apps-with-curl).

Le profil enregistre LoRA, graine/aléatoire, guidance et étapes ; **Créer avec l’IA…**
permet de les changer pour cette génération sans écraser les valeurs du profil.
La liste LoRA reprend exactement ces noms du serveur :

- Multiple-Angles, Photo-to-Anime, Anime-V2, Light-Migration, Upscaler ;
- Style-Transfer, Manga-Tone, Anything2Real, Fal-Multiple-Angles, Polaroid-Photo ;
- Unblur-Anything, Midnight-Noir-Eyes-Spotlight, Hyper-Realistic-Portrait ;
- Ultra-Realistic-Portrait, Pixar-Inspired-3D, Noir-Comic-Book ;
- Any-light, Studio-DeLight, Cinematic-FlatLog.

Le champ reste éditable pour un futur adaptateur. **Photo-to-Anime**, guidance 1,
4 étapes et graine aléatoire sont proposés au départ. **Sans LoRA** envoie
`lora_adapter: null` et propose une guidance de 4 et 40 étapes. Ces réglages doivent
correspondre aux capacités et adaptateurs installés sur le serveur. La commande
ne passe ni par l’interface Gradio ni par ComfyUI. Une sélection est envoyée à sa
boîte englobante puis réappliquée comme masque local ; le serveur ne reçoit pas
un masque d’inpainting distinct. Le résultat peut devenir un nouveau calque
annulable ou un nouveau document à résolution native, avec conservation du masque
et de la profondeur 8/16 bits de PhotoCrap. Cela ne crée pas des détails 16 bits
supplémentaires si le PNG généré est en 8 bits.

**Annuler** arrête l’attente et empêche toute insertion tardive ; cette API ne
fournit pas ici de commande d’arrêt distant, le calcul peut continuer sur le
serveur. Les tests utilisent un serveur simulé pour l’authentification, le JSON,
les événements fragmentés, le résultat, les erreurs et l’annulation. Une réponse
401 a été constatée sans identifiants sur le serveur local. Une première génération
réelle a été confirmée dans PhotoCrap ; les essais prolongés restent à reprendre
après rétablissement du serveur, devenu injoignable même sans envoi d’image.
La cause de cette indisponibilité doit être vérifiée dans les logs du serveur.

## Traitements sans prompt et upscale Topaz

Un profil fal.ai existant dont le modèle est `topaz/upscale/image/precision`
est reconnu automatiquement, même s’il a été créé manuellement ou renommé.
Ses champs sont corrigés à l’ouverture et à l’enregistrement : `image_url`
singulier, mode sans prompt, retrait des anciens champs de génération et envoi
natif pour les profils hérités. Son identifiant, son nom, son serveur et sa
référence de clé sont conservés ; cette migration ne lit pas le trousseau.
Les options Topaz personnalisées restent présentes. Une erreur HTTP structurée
indique désormais le champ et le message de validation, sans recopier l’image
ni révéler la clé API.

Les profils fal.ai et ComfyUI proposent un mode **Obligatoire**, **Facultatif** ou
**Sans prompt** dans les réglages. Les anciens profils restent en mode obligatoire.
Sans prompt, le dialogue masque l’éditeur et les modèles de texte ; son bouton
**Lancer le traitement** n’exige aucun texte. Pour fal.ai, le champ `prompt` est
omis ; `num_images` n’est plus injecté pour ces traitements. Les profils OpenAI
et compatibles conservent le prompt obligatoire de leur protocole d’édition.

1. Dans **Réglages → IA → +**, choisir **fal.ai · Topaz Upscale Precision**.
2. Utiliser la clé partagée d’un profil fal.ai existant, ou renseigner une clé propre
   au profil dans le trousseau ou via `FAL_KEY`, puis enregistrer.
3. Dans **Créer avec l’IA…**, sélectionner ce profil. Le modèle Topaz et le facteur
   d’agrandissement (×1 à ×4, avec valeurs intermédiaires) se règlent dans le dialogue.
4. Après une génération IA, sélectionner son calque : la source **Calque actif ·
   image IA d’origine** est proposée et sélectionnée par défaut pour Topaz. Elle
   envoie les pixels originaux du fournisseur à leur petite résolution, plutôt
   que leur version déjà agrandie pour l’affichage. Après retouche des pixels,
   cette source n’est plus proposée ; la composition ou la sélection restent disponibles.

Le preset appelle `topaz/upscale/image/precision` avec `image_url` comme valeur
unique, sans prompt ni champ `num_images`. Il utilise Standard V2, ×2 et PNG par
défaut ; les options avancées permettent les paramètres supplémentaires du modèle.
La réponse singulière `image.url` et la file asynchrone sont prises en charge.
[Schéma officiel fal.ai](https://fal.ai/models/topaz/upscale/image/precision/api).

L’envoi natif ne réduit pas l’image et conserve sa profondeur PNG lorsqu’elle est
16 bits. Pour les autres profils, la limite d’envoi configurée reste disponible.
La destination **Nouveau document · résolution native**, proposée par défaut
pour l’upscale, conserve les dimensions renvoyées par le fournisseur dans un nouvel
onglet. **Nouveau calque dans ce document** conserve le placement et les dimensions
du canevas actuel, avec une étape d’annulation. Les deux destinations conservent
le masque, les octets originaux du résultat et les informations de modèle/facteur.
Le masque et son contour progressif sont remis à l’échelle pour le nouveau document,
avec activation et densité conservées. [Documents multiples](DOCUMENTS.md).

Les limites actuelles du lecteur IA restent 32 Mpx et 16 384 px par côté : une
sortie Topaz prévue au-delà est refusée avant soumission. Réduire le facteur ou
choisir l’image IA originale. Les limites d’encodage d’entrée (50 Mio pour l’image,
4 Mio pour le masque) et de téléchargement (100 Mio) restent actives. Aucun appel
réel Topaz n’a été effectué pendant les tests automatisés ; les échanges sont simulés.

## Autre upscale : Real-ESRGAN

**Réglages → IA → + → fal.ai · Real-ESRGAN** crée un profil distinct de Topaz.
Il utilise `fal-ai/esrgan`, sans prompt, avec `RealESRGAN_x2plus`, `scale: 2`,
`face: false` et une sortie PNG. La restauration des visages est désactivée par
ce preset pour éviter une modification supplémentaire des traits. Le modèle et
le facteur restent modifiables dans **Créer avec l’IA…**. Les capacités connues
sont également reconnues si le chemin du modèle est saisi manuellement.

Comme Topaz, ce profil sélectionne les pixels IA d’origine du calque lorsqu’ils
sont disponibles. La destination permet soit un nouveau calque avec son placement et son masque,
soit un nouveau document à la résolution native de la sortie (proposé par défaut). Les limites locales de sortie et
le facteur proposé ×1–×4 sont vérifiés avant envoi. La clé fal.ai se configure
pour ce nouveau profil dans le trousseau ou via `FAL_KEY`, ou se retrouve depuis
un unique profil fal.ai du même serveur portant déjà une clé.

Ce choix est une alternative à comparer sur la même image, sans garantie de
supériorité ou de fidélité colorimétrique : Real-ESRGAN peut aussi modifier les
textures. Aucun appel réel à ce modèle n’a été effectué pour cette intégration.
Les tests de transport vérifient séparément qu’une réponse PNG identique conserve
les couleurs après conversion sRGB, en 8/16 bits, depuis sRGB ou Display P3 ; ils
ne mesurent pas les changements introduits par le modèle distant.

[Schéma officiel fal.ai](https://fal.ai/models/fal-ai/esrgan/api) : le facteur se
nomme `scale` pour cet endpoint, contrairement à `upscale_factor` chez Topaz.
La réponse contient une image unique (`image.url`).

## Serveur ComfyUI

Indiquer le nom exact d’un checkpoint installé, par exemple celui d’un modèle
SD/SDXL compatible avec le workflow fourni. `model.safetensors` est un emplacement
à remplacer, pas un modèle téléchargé automatiquement. Le graphe par défaut
encode l’image, applique le masque de bruit à la sélection et utilise un KSampler
avec un débruitage de 0,55. Modifier le workflow pour changer ce comportement ou
utiliser une autre architecture.

Un workflow personnalisé doit être exporté **au format API**, et non au format
visuel contenant `nodes`. Dans ses entrées JSON, les valeurs chaînes suivantes
sont remplacées lorsqu’elles constituent la valeur entière :

| Valeur | Remplacement |
| --- | --- |
| `{{image}}` | Nom de l’image source envoyée au serveur |
| `{{mask}}` | Nom du PNG masque envoyé au serveur |
| `{{prompt}}` | Texte du prompt |
| `{{model}}` | Nom du checkpoint configuré |
| `{{width}}`, `{{height}}` | Dimensions envoyées, valeurs numériques |
| `{{seed}}` | Graine numérique aléatoire |

Prévoir un nœud `SaveImage` ou `PreviewImage`. Le premier résultat image du
workflow devient le nouveau calque. Les nœuds personnalisés doivent être installés
sur le serveur. Le masque PNG a un alpha nul dans la zone modifiable ; `LoadImage`
convertit cet alpha en masque pour le workflow fourni.

### Peindre sur une surface

1. Dans ComfyUI, installer `moge_2_vitl_normal_fp16.safetensors` dans
   `models/geometry_estimation/`. Utiliser une version qui fournit
   `LoadMoGeModel`, `MoGeInference` et `MoGeRender`.
   [Guide ComfyUI et téléchargement du modèle](https://docs.comfy.org/tutorials/utility/moge)
2. Dans PhotoCrap, ouvrir **Calque → 3D → Peindre sur une surface (ComfyUI)**. L’analyse
   démarre directement si un seul serveur ComfyUI est configuré ou si un choix
   est déjà mémorisé. Pour changer de profil, utiliser **Choisir le serveur de
   peinture…** dans le même menu. PhotoCrap utilise son propre workflow et
   envoie la composition visible au serveur choisi, avec un grand côté limité
   à 2 048 px.
3. Cliquer sur la surface à peindre, puis utiliser le pinceau. PhotoCrap ajoute
   un calque de pixels et son masque éditable dans une seule étape d’annulation.
   Le mode de fusion **Couleur** conserve la luminosité de la photo et peut être
   changé dans le panneau Calques.

Le masque suit la zone connectée au clic selon la disparité et l’orientation de
la surface. Les discontinuités estimées arrêtent la propagation, par exemple au
contour d’un sujet devant un mur. La qualité des bordures dépend des cartes
produites par MoGe et de leur résolution. L’analyse se lance une fois pour
préparer le masque. Les coups de pinceau se font ensuite dans PhotoCrap.

**Calque → 3D → Peindre sur une surface (en local)** lance MoGe‑2 ViT‑S sur le
CPU avec ONNX Runtime. Au premier lancement, PhotoCrap télécharge et vérifie
le modèle de 141 Mo. Il est stocké dans le dossier de données de l’application
et les analyses suivantes fonctionnent hors ligne. La photo est traitée sur
l’ordinateur. Le grand côté de l’image d’analyse est limité à 768 px. Le clic,
le masque éditable et le pinceau fonctionnent comme avec ComfyUI.
[Modèle MoGe‑2 ViT‑S ONNX](https://huggingface.co/Ruicheng/moge-2-vits-normal-onnx)

## Variantes

Le dialogue propose **1 à 8 variantes** par lancement, sauf les upscales Topaz et Real-ESRGAN, limités à une image. Elles utilisent toutes
la même capture de l’image ou de la sélection et le même prompt, même si le
document est retouché pendant l’attente. Une seule requête est active à la fois :
chaque variante est une génération distincte chez le fournisseur. Avec le workflow
ComfyUI fourni, une nouvelle graine aléatoire est injectée à chaque variante.
Un workflow personnalisé ou des options imposant une graine fixe peuvent produire
des résultats identiques.

Chaque variante est ajoutée sur un calque numéroté. La plus récente est visible ;
les précédentes du même lancement sont masquées, disponibles via l’œil du panneau
Calques. Chaque ajout a sa propre étape d’annulation, qui restaure aussi la
visibilité de la variante précédente. Annuler une génération ou rencontrer une
erreur arrête les variantes restantes et conserve les calques déjà produits.
Si l’insertion est bloquée par une limite, la suite attend l’ajout du résultat
conservé avant d’envoyer la variante suivante.

Le nom des nouveaux calques indique le fournisseur, le modèle et **Variante 1/3**,
par exemple. Même un lancement simple porte **1/1**. La seconde ligne du panneau
Calques rappelle le fournisseur et ce numéro ; survoler le nom affiche les détails
complets, la date et le prompt. Ces informations restent dans le projet après
renommage du calque. Les anciens calques IA disposant de ces métadonnées affichent
aussi ces détails, sans changer leur nom existant.

Une même variante ne peut être insérée deux fois : les notifications répétées et
les nouvelles tentatives d’insertion ne dupliquent pas son calque. Deux variantes
explicitement demandées peuvent néanmoins être identiques si le fournisseur
renvoie le même résultat, notamment avec une graine fixe dans un workflow local.

## Suivi de la génération

Un encadré contrasté apparaît en haut du canevas pendant la génération. Il affiche
le fournisseur et le modèle, l’étape courante en entier, le temps écoulé et le
nombre de variantes réellement créées. La barre animée signale l’attente ; elle
n’annonce pas un pourcentage que le fournisseur ne transmet pas. **Annuler** reste
directement accessible et le reste du canevas reste utilisable.

La confirmation de fin reste affichée huit secondes. Une erreur reste visible
jusqu’à sa fermeture. Si un résultat attend son insertion, l’encadré reste présent
avec **Ajouter le résultat** et **Annuler**.

## Image source et résultat

Par défaut, la source est la **composition visible** capturée au lancement. Pour une
sélection, seul son rectangle englobant est envoyé, avec la forme de sélection
et les pixels extérieurs transparents. Le grand côté est limité par configuration
à 256–4096 px (1536 par défaut, 1024 pour ComfyUI). Sans l’option native, l’envoi
utilise du PNG 8 bits, même depuis un document 16 bits. L’option **Envoyer à la
résolution native** contourne cette réduction et conserve le PNG 16 bits lorsque
la source le permet ; elle est activée par défaut pour Topaz et Real-ESRGAN. La source IA originale
du calque est également disponible lorsqu’elle est encore valide. Le masque suit la convention alpha transparent
pour la zone à modifier ; le fournisseur peut interpréter ce masque différemment
ou ne pas le prendre en charge.

Le résultat est ajusté aux dimensions et à la position de la zone source et placé
sur un **nouveau calque de pixels**, au-dessus du calque initialement actif.
Avec une sélection, un masque conserve sa forme et son contour progressif,
appliqué une seule fois au résultat. Le document peut rester en 8 ou 16 bits ;
convertir une sortie IA 8 bits en 16 bits ne recrée pas de précision perdue.
La sélection courante est conservée. **Annuler** retire le calque en une étape.
Le fournisseur, le modèle, le prompt et la date sont conservés dans les métadonnées
du calque, sans clé API.

La préparation, les requêtes et la reconstruction du calque sont asynchrones.
On peut poursuivre une retouche pendant l’attente ; l’insertion attend la fin du
geste en cours. Si la limite de calques ou de stockage est atteinte, le résultat
reste en mémoire : libérer de la place puis cliquer **Ajouter le résultat**, sans
relancer un appel au fournisseur. Ce résultat en attente n’est pas sauvegardé à
la fermeture de l’application.

Changer de document ou ses dimensions abandonne la demande pour éviter une
insertion au mauvais endroit. **Annuler** interrompt le suivi local et tente
l’annulation du travail fal.ai ou de la seule tâche ComfyUI encore en file.
Un travail déjà en cours peut continuer sur le serveur ; l’annulation ne garantit
pas l’arrêt de la facturation. PhotoCrap n’interrompt jamais tous les travaux
ComfyUI avec la commande globale `/interrupt`.

## Stockage des résultats dans le projet

Les projets **v13** conservent les octets de l’image renvoyée par le fournisseur
et son rectangle de placement tant que les pixels du calque n’ont pas été
retouchés. Le masque, l’opacité et la visibilité restent enregistrés séparément.
À l’ouverture, le calque est reconstruit localement à la résolution et à la
profondeur du document, sans appel IA et sans compression JPEG supplémentaire.
La sauvegarde ne réencode plus inutilement la petite sortie IA en PNG 16 bits
agrandi à toute la taille du document.

Une modification des pixels invalide cette source compacte : la sauvegarde
utilise alors le raster natif complet pour conserver la retouche. Annuler cette
modification peut restaurer la source compacte. La modification du masque seule
conserve l’optimisation. Les données sources partagées sont comptées une seule fois
dans le suivi mémoire et dans l’historique.

Les calques générés **avant ce correctif**, puis enregistrés comme rasters, ne
contiennent plus la sortie du fournisseur à sa taille d’origine : leur provenance
IA ne permet pas de la retrouver. Ils restent sauvegardés sans réduction de leur
précision. Le nouveau stockage concerne les résultats dont la source est encore
conservée. Les anciens projets restent lisibles ; un projet utilisant cette source
compacte nécessite le lecteur v13. Les exports aplatis PNG/JPEG sont inchangés.

## Limites et validation

Les essais réels OpenAI et fal.ai ont été confirmés par l’utilisateur le
8 novembre 2026. ComfyUI reste à essayer avec son serveur et son workflow ;
son protocole est couvert par le serveur simulé des tests.

Un lancement utilise un prompt et produit jusqu’à huit variantes successives.
Les bibliothèques contiennent plusieurs prompts ; les lots de prompts différents,
conversations et aperçus en streaming ne sont pas encore implémentés. Les entrées sont limitées à 50 Mio pour
l’image et 4 Mio pour le masque ; les réponses à 100 Mio, 32 Mpx et 16384 px par
axe, en PNG/JPEG/WebP. Une requête a un délai réseau de 180 secondes et le suivi
d’une génération est limité à 15 minutes.

Les tests utilisent un serveur HTTP simulé : multipart OpenAI, file fal.ai,
workflow ComfyUI, authentification, redirections, erreurs, annulation, insertion
8/16 bits et interface. Aucun appel payant ni serveur ComfyUI réel n’est nécessaire
pour les exécuter. La compatibilité réelle dépend du modèle, de ses droits et du
workflow installés ; elle reste à essayer avec les configurations de l’utilisateur.

Références officielles : [édition d’images OpenAI](https://developers.openai.com/api/reference/resources/images/methods/edit),
[génération d’images OpenAI](https://developers.openai.com/api/docs/guides/image-generation),
[file fal.ai](https://fal.ai/docs/documentation/model-apis/inference/queue),
[FLUX.2 Edit](https://fal.ai/models/fal-ai/flux-2/edit/api),
[routes du serveur ComfyUI](https://docs.comfy.org/development/comfyui-server/comms_routes)
et [exemple officiel de soumission](https://github.com/Comfy-Org/ComfyUI/blob/master/script_examples/websockets_api_example.py).

Stockage sécurisé : [QtKeychain officiel](https://github.com/frankosterfeld/qtkeychain), version 0.15.0 liée statiquement et vérifiée par SHA-256 ; licence BSD distribuée avec l’application.

## Destination et documents multiples

**Destination → Nouveau document · résolution native** conserve les dimensions
renvoyées par le fournisseur et crée un onglet indépendant, non enregistré.
L’historique et les pixels du document source restent inchangés. Le masque est
redimensionné pour le nouveau canevas. Cette destination est proposée par défaut
pour Topaz et Real-ESRGAN ; les autres profils proposent le nouveau calque.
Chaque onglet conserve son contrôleur IA, y compris lorsqu’un autre document est
actif. Un résultat terminé en arrière-plan n’interrompt pas l’édition de l’autre
onglet. [Comportements de fermeture et limites](DOCUMENTS.md).
