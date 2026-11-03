# Texte éditable : fonctions et références Adobe

## Rendu régional — 3 novembre 2026

La rasterisation se limite désormais à l’emprise du texte dans le canevas et écrit
un plan tuilé natif 8/16 bits. Elle conserve les débordements de caractères et le
clipping des paragraphes. Saisie, formats mixtes, déplacement et redimensionnement
utilisent ce chemin ; les paramètres éditables et le format de projet ne changent
pas. Revenir à l’ancre pendant un déplacement restaure le snapshot sans étape
supplémentaire. Le texte hors canevas reste éditable et compact.

## Utilisation

Choisir **Texte (T)** puis cliquer dans l’image ouvre un texte ponctuel vide.
Glisser crée un bloc de paragraphe avec retour automatique à la ligne. La saisie,
le curseur et la sélection se font sur le canevas ; le texte d’exemple n’est plus
inséré. Maj-clic force la création d’un nouveau texte plutôt que de reprendre
un texte situé sous le pointeur.

La vignette **T** désigne un vrai calque de texte. Double-cliquer dessus, ou sur
sa ligne, reprend son édition ; **Renommer** reste une action distincte du menu
contextuel et ne modifie pas les lettres sur l’image. Le renommage manuel est
conservé lors des éditions suivantes. Avec Texte actif, le survol garde un curseur
de saisie et un clic dans un texte existant reprend l’édition à l’endroit cliqué,
y compris après le relâchement de la souris, sans créer un autre calque.
Le menu contextuel offre **Modifier le texte…** et
**Pixelliser le texte** ; cette dernière action est annulable.

**Caractère / Paragraphe…** ouvre les réglages du texte en cours. Une sélection
de caractères reçoit la mise en forme ; sans sélection, elle s’applique au texte
entier. Entrée insère une nouvelle ligne ; Commande/Ctrl–Entrée ou **Valider**
termine la session. Échap ou **Annuler** restaure toute la session. Le moteur Qt
fournit l’édition, la sélection, le presse-papiers et les méthodes de saisie ;
les raccourcis d’outils sont suspendus pendant cette édition. Commande/Ctrl–B,
–I et –U appliquent gras, italique et soulignement. Annuler pendant la saisie
utilise l’historique interne de texte ; après validation, un seul Annuler du
document retire toute la session (et le calque s’il vient d’être créé).

Pendant l’édition, **Commande/Ctrl-glisser** à l’intérieur du texte déplace le
calque sans valider la saisie. Le contour peut aussi être tiré sans modificateur.
Le curseur de déplacement indique ces zones, tandis qu’un clic ordinaire dans
les lettres conserve le placement du curseur de saisie. Avec **Déplacement (V)**,
le texte sélectionné conserve un cadre visible ; glisser le déplace et les
flèches ajustent sa position de 1 pixel, ou 10 avec Maj. Le déplacement effectué
pendant la saisie fait partie de la même session annulable que son contenu.

Le bloc de paragraphe peut être redimensionné par sa poignée inférieure droite
ou par ses champs Largeur/Hauteur. Le signe rouge **+** indique un contenu qui
ne tient pas dans le bloc. Le passage texte ponctuel/paragraphe reste éditable.
L’outil Déplacement, le redimensionnement de l’image, les rotations/miroirs et
les recadrages conservent la définition du texte et sa transformation. Les
masques, l’opacité et la fusion utilisent le composite natif, y compris pendant
la saisie ; le composant d’entrée ne dessine pas une deuxième copie des lettres.

## Couverture par rapport à Photoshop

| Fonction de référence | État PhotoCrap |
| --- | --- |
| Texte ponctuel, texte de paragraphe, multiligne, reprise d’un texte | Disponible |
| Nouveau calque dédié, double-clic sur sa vignette, nom indépendant | Disponible |
| Familles installées et styles de police ; taille en pixels | Disponible |
| Gras, italique, souligné, barré, couleur sur tout ou partie du texte | Disponible |
| Capitales, petites capitales, exposant et indice | Disponible |
| Approche, crénage métrique, échelle horizontale, décalage de ligne de base | Disponible ; métriques du moteur Qt, sans identité de rendu Adobe |
| Interligne auto/fixe, alignements gauche/centre/droite/justifié | Disponible |
| Retraits gauche/droit/première ligne, espacements avant/après, direction RTL | Disponible |
| Presse-papiers texte, sélection et saisie Unicode/IME | Moteur Qt ; les objets/images HTML externes ne sont pas importés |
| Déplacement du calque et conservation après transformations du document | Disponible ; transformation libre interactive du calque encore absente |
| Conversion ponctuel/paragraphe, taille du bloc, indicateur de débordement | Disponible ; une poignée de taille, sans jeu complet de poignées Photoshop |
| Enregistrement du texte, de formats mixtes et de transformations | Projet v9, profondeur du cache natif 8/16 bits |
| Pixellisation explicite, Annuler/Rétablir, duplication du calque | Disponible |
| Texte sur tracé | NYI |
| Déformations de texte (Arc, Arche, Drapeau, etc.) | NYI |
| Texte vertical et masques de texte | NYI |
| Glyphes, fonctions OpenType explicites, axes de polices variables | NYI ; le moteur système peut appliquer ses fonctions de composition par défaut |
| Crénage optique et ajustement manuel par paire | NYI |
| Modes de lissage Adobe Aucun/Net/Précis/Fort/Léger | NYI ; anticrénelage Qt |
| Échelle verticale de caractères et transformation libre complète | NYI |
| Styles de caractère/paragraphe réutilisables | NYI |
| Césure, justification détaillée, compositeurs Adobe, typographie CJK avancée | NYI |
| Texte dynamique, détection de police, Adobe Fonts et incorporation de polices | NYI |
| Correction orthographique, recherche/remplacement dans les calques de texte | NYI |

Ces fonctions ne constituent donc pas encore l’ensemble des fonctions de texte
de Photoshop. Les entrées avancées du panneau restent marquées **NYI**.

## Stockage et rendu

Le type de calque `text` conserve le contenu HTML normalisé, les formats de
caractères/blocs non représentables dans le HTML Qt, la taille du bloc et une
matrice de transformation. Le PNG n’est qu’un cache de pixels pour le composite ;
modifier le texte le reconstruit à partir de sa définition. La recomposition
pendant la saisie est limitée à l’union des anciennes/nouvelles zones de texte.
Le cache couleur complet est encore alloué lors du rendu d’un texte non vide.

Le projet v9 conserve cette définition et les formats mixtes ; les projets v1–v8
restent lisibles. Les anciens exécutables limités à v8 ne lisent pas les projets
v9. Les textes déjà rasterisés dans les anciennes versions restent des pixels :
leur contenu et leurs polices ne peuvent pas être reconstitués automatiquement.
Les polices ne sont pas embarquées ; une police manquante est remplacée par le
système et signalée dans le panneau, et sa composition peut changer à l’édition.
Les détails de métrique et de crénage dépendent de Qt et des polices installées.

Les données importées sont bornées et normalisées. Les images, ressources
externes et cadres HTML imbriqués ne sont pas chargés comme contenu de texte.
Les formats numériques sont reconstruits avec les types Qt corrects après JSON,
pour conserver notamment le décalage de ligne de base.

## Références officielles

- [Ajouter du texte](https://helpx.adobe.com/photoshop/desktop/text-typography/get-started-with-text/add-text.html)
- [Modifier le texte](https://helpx.adobe.com/photoshop/desktop/text-typography/get-started-with-text/edit-text.html)
- [Déplacer le texte](https://helpx.adobe.com/photoshop/desktop/text-typography/get-started-with-text/move-text.html)
- [Raccourcis de texte de la référence Adobe](https://helpx.adobe.com/content/dam/help/en/pdf/photoshop_reference.pdf)
- [Calques de texte et éditabilité](https://helpx.adobe.com/photoshop/desktop/text-typography/type-layers-creation/type-layers-in-photoshop.html)
- [Format des caractères](https://helpx.adobe.com/photoshop/desktop/text-typography/characters-glyphs/format-characters.html)
- [Format des paragraphes](https://helpx.adobe.com/photoshop/desktop/text-typography/characters-glyphs/format-paragraphs.html)
- [Interligne, crénage et approche](https://helpx.adobe.com/photoshop/desktop/text-typography/characters-glyphs/adjust-line-and-character-spacing.html)
- [Propriétés de caractère de l’API Photoshop](https://developer.adobe.com/photoshop/uxp/2022/ps-reference/classes/characterstyle)

## Vérifications

`text_tools` couvre le contenu éditable, les formats mixtes et les propriétés
natives, les projets, l’annulation, le texte ponctuel/paragraphe, les transformations,
la pixellisation et le rejet des objets externes en 8/16 bits. Le test `tool_layers`
utilise la saisie dans la fenêtre Qt, le double-clic sur la vignette et la mise en
forme d’une sélection ; il vérifie aussi l’annulation sans calque supplémentaire.
Ce même scénario passe dans une fenêtre native macOS avec Metal (boucle de rendu
threadée) et OpenGL (boucle simple). Le contrôle couvre aussi la taille de police
après mise en forme et sauvegarde, ainsi que la réouverture après l’annulation
d’un autre texte, pour éviter de réinjecter un ancien contenu d’éditeur.
Le survol et le clic simple sur un texte existant sont vérifiés à plusieurs
zooms. Sur macOS, le test contrôle aussi `NSCursor.currentCursor`, car la forme
conservée par Qt ne suffit pas à garantir le curseur réellement affiché par AppKit.
