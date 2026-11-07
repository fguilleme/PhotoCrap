# Création de calques avec une IA

**Fichier → Créer avec l’IA…** et **Calques → Créer avec l’IA…** ouvrent le même
éditeur. Choisir une configuration de fournisseur, la composition visible entière
ou la sélection, puis un prompt enregistré ou un texte libre. **Générer** lance
l’envoi ; ouvrir le dialogue ou changer de preset n’envoie rien.

## Fournisseurs et prompts

Dans **Réglages → IA**, deux onglets permettent d’enregistrer plusieurs
configurations de fournisseurs et une bibliothèque de prompts réutilisables.
La liste des fournisseurs reste visible à gauche. **+ → OpenAI / fal.ai / ComfyUI / API compatible** crée immédiatement un profil. Compléter sa configuration puis utiliser **Enregistrer**, toujours visible en bas à droite. Un message confirme la sauvegarde ou indique précisément l’erreur. On peut ainsi
conserver plusieurs modèles OpenAI, endpoints fal.ai ou serveurs locaux.

| Type | Configuration | Protocole |
| --- | --- | --- |
| OpenAI · GPT Image | Adresse terminant par `/v1`, modèle accessible au compte, clé API | `POST /images/edits`, image PNG et masque multipart, résultat encodé ou URL |
| fal.ai | Serveur de file, chemin du modèle d’édition | Envoi JSON puis suivi de la file et téléchargement du résultat |
| ComfyUI | Adresse du serveur, checkpoint installé, workflow au format API | Upload des images, soumission du workflow, suivi de son historique, récupération de l’image |
| API compatible OpenAI | Serveur implémentant réellement l’édition d’images OpenAI | Même protocole multipart que GPT Image ; une API de chat seule ne suffit pas |

Les configurations initiales utilisent `gpt-image-2.5-sunburst`,
`fal-ai/flux-2/edit` et un serveur ComfyUI à `http://127.0.0.1:8188`.
Les noms de modèles restent modifiables selon les droits du compte ou le serveur.
PhotoCrap n’installe ni modèle ni serveur local.

La clé API saisie puis enregistrée est conservée par **QtKeychain** dans le
trousseau macOS, le gestionnaire d’identifiants Windows ou le service de secrets
Linux (GNOME Keyring/libsecret ou KWallet). Elle est retrouvée au lancement suivant,
sans apparaître dans les champs, les réglages en clair ou les projets. L’interface
indique **Clé enregistrée** ; laisser le champ vide conserve la clé existante.
Les variables d’environnement (`OPENAI_API_KEY`, `FAL_KEY` par défaut) restent
une alternative, visible dans le statut du fournisseur.

La suppression du fournisseur ou la modification de son adresse/protocole supprime
sa clé enregistrée. Les identifiants sont liés à l’UUID du profil et à son endpoint.
Les téléchargements d’images externes ne reçoivent pas la clé du fournisseur.
**Options avancées → Supprimer la clé enregistrée** permet de l’effacer séparément.
Si le trousseau est indisponible ou verrouillé, l’interface affiche l’erreur ; aucun
repli en stockage clair n’est autorisé. Sous Linux, installer `libsecret-1` et
activer le service de secrets de la session. Le build requiert `libsecret-1-dev`.

Depuis la génération, **Gérer les fournisseurs…** ouvre cette page. Après
l’enregistrement, **Fermer** ramène à la génération, avec le profil sélectionné et
le prompt conservé. Aucun envoi n’a lieu pendant la configuration.

Les options avancées acceptent un objet JSON pour les paramètres du modèle.
Le prompt, l’image, le modèle et le nombre de résultats restent gérés par PhotoCrap.
Pour fal.ai, le nom du champ image et son caractère liste/valeur sont configurables
pour adapter les modèles d’édition. Les options ne rendent pas interchangeables
les schémas des différents modèles.

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

## Variantes

Le dialogue propose **1 à 8 variantes** par lancement. Elles utilisent toutes
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

La source est la **composition visible** capturée au lancement. Pour une
sélection, seul son rectangle englobant est envoyé, avec la forme de sélection
et les pixels extérieurs transparents. Le grand côté est limité par configuration
à 256–4096 px (1536 par défaut, 1024 pour ComfyUI). L’envoi utilise du PNG 8 bits,
même depuis un document 16 bits. Le masque suit la convention alpha transparent
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
