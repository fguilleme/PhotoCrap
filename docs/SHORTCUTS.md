# Raccourcis clavier et fonctions manquantes

Référence des raccourcis Photoshop pertinents pour l’édition d’images dans PhotoCrap : documents, sélection, calques, couches, peinture, fusion, affichage et outils. Les anciennes fonctions 3D, DICOM, vidéo et les modules externes de Photoshop ne font pas partie de ce périmètre.

Sources Adobe :
- [Référence Photoshop Desktop](https://helpx.adobe.com/photoshop/desktop/get-started/settings-and-preferences/view-keyboard-shortcuts.html).
- [Manuel officiel Photoshop CS6 archivé, pages 71–95](https://help.adobe.com/archive/en/photoshop/cs6/photoshop_reference.pdf), pour les tableaux détaillés de gestes, peinture, fusion, couches et panneaux. Ces tableaux anciens servent de référence aux gestes historiques ; ils ne décrivent pas les nouveautés de Photoshop.
- [Commandes clavier Photoshop web](https://helpx.adobe.com/photoshop/web/get-set-up/preferences-and-settings/keyboard-shortcuts.html), pour recouper les commandes courantes. La liste web n’est pas assimilée à celle de Desktop : notamment Ctrl/Commande+2 affiche le composite RVB dans PhotoCrap.

Dans Réglages → Raccourcis clavier, toutes les entrées sont consultables et filtrables : disponibles, NYI ou gestes contextuels. Les commandes disponibles sont personnalisables et enregistrées automatiquement ; les gestes et entrées NYI restent des indications non éditables. Une entrée NYI ne réserve ni ne déclenche sa combinaison. Les conflits des commandes disponibles et des changements d’outils avec Maj sont détectés.

Les notations Ctrl et Alt des champs de configuration correspondent à Commande et Option sur macOS ; les indications de combinaisons simples utilisent l’affichage natif. Les flèches, Espace, Maj et Alt pendant un geste restent des modificateurs contextuels.

Particularités PhotoCrap conservées : Cmd/Ctrl+C copie l’image visible, Cmd/Ctrl+J duplique le calque complet et Cmd/Ctrl+Maj+N crée directement le calque. Les variantes Photoshop qui copient/coupent les pixels du calque ou de la sélection sont donc recensées séparément en NYI. Maj+Retour arrière et Maj+F5 ouvrent un sélecteur de couleur pour remplir les pixels ou le masque ; le dialogue de remplissage complet de Photoshop reste NYI. Les réglages directs Courbes/Niveaux/Noir et blanc sont NYI pour leurs raccourcis : leurs panneaux existent comme calques de réglage, ce qui est une autre opération.

Les modes de fusion s’appliquent au pinceau, crayon, gomme, tampons et formes d’historique, ou au calque avec Déplacement/Main/Zoom/sélection. Éponge et Densité prennent la priorité sur leurs combinaisons partagées pour régler saturation et plage tonale. Les limites actuelles des outils en 16 bits restent applicables.

Tab et Maj+Tab masquent temporairement les panneaux, y compris flottants, sans changer l’organisation enregistrée. Tab masque également la barre latérale des outils. La barre supérieure des options reste affichée ; son masquage à l’identique de Photoshop est recensé en NYI.

La Gomme magique est maintenant disponible dans le groupe E : Maj+E parcourt les variantes, le clic droit permet de la choisir directement. Sa combinaison suit celle du groupe Gomme personnalisable ; son entrée descriptive n’enregistre pas un second raccourci E.

## Commandes ajoutées

| Action | Combinaison par défaut | Contexte |
| --- | --- | --- |
| Configurer les raccourcis | Ctrl+Alt+Shift+K | Document |
| Copier avec fusion | Ctrl+Shift+C | Document |
| Intervertir la sélection | Ctrl+Shift+I | Document |
| Resélectionner | Ctrl+Shift+D | Document |
| Contour progressif | Shift+F6 | Document |
| Plage de couleurs | Aucun raccourci par défaut | Document |
| Intervertir (touche de fonction) | Shift+F7 | Document |
| Grouper les calques | Ctrl+G | Document |
| Dissocier les calques | Ctrl+Shift+G | Document |
| Créer/libérer le masque d’écrêtage | Ctrl+Alt+G | Document |
| Monter le calque | Ctrl+] | Document |
| Descendre le calque | Ctrl+[ | Document |
| Sélectionner le calque supérieur | Alt+] | Document |
| Sélectionner le calque inférieur | Alt+[ | Document |
| Sélectionner le premier calque | Alt+. | Document |
| Sélectionner le dernier calque | Alt+, | Document |
| Sélectionner tous les calques | Ctrl+Alt+A | Document |
| Nouveau calque sans dialogue | Ctrl+Alt+Shift+N | Document |
| Taille de l’image | Ctrl+Alt+I | Document |
| Négatif | Ctrl+I | Document |
| Zoom avant | Ctrl++ | Document |
| Zoom arrière | Ctrl+- | Document |
| Défiler vers le haut | PgUp | Document |
| Défiler vers le bas | PgDown | Document |
| Défiler vers la gauche | Ctrl+PgUp | Document |
| Défiler vers la droite | Ctrl+PgDown | Document |
| Aller en haut à gauche | Home | Document |
| Aller en bas à droite | End | Document |
| Afficher/masquer les panneaux | Shift+Tab | Document |
| Afficher/masquer panneaux et outils | Tab | Document |
| Panneau Couleur | F6 | Document |
| Panneau Calques | F7 | Document |
| Panneau Informations | F8 | Document |
| Options du pinceau | F5 | Document |
| Afficher RVB | Ctrl+2 | Document |
| Afficher Rouge | Ctrl+3 | Document |
| Afficher Vert | Ctrl+4 | Document |
| Afficher Bleu | Ctrl+5 | Document |
| Remplir : premier plan | Alt+Backspace | Document |
| Remplir : arrière-plan | Ctrl+Backspace | Document |
| Remplir : premier plan, transparence conservée | Alt+Shift+Backspace | Document |
| Remplir : arrière-plan, transparence conservée | Ctrl+Shift+Backspace | Document |
| Remplir avec une couleur… | Shift+Backspace | Document |
| Remplir (touche de fonction) | Shift+F5 | Document |
| Mode de fusion suivant | Shift++ | Calque ou outil de peinture |
| Mode de fusion précédent | Shift+- | Calque ou outil de peinture |
| Normal | Alt+Shift+N | Calque ou pinceau · priorité Éponge/Densité |
| Fondu | Alt+Shift+I | Calque ou pinceau · priorité Éponge/Densité |
| Obscurcir | Alt+Shift+K | Calque ou pinceau · priorité Éponge/Densité |
| Produit | Alt+Shift+M | Calque ou pinceau · priorité Éponge/Densité |
| Densité couleur + | Alt+Shift+B | Calque ou pinceau · priorité Éponge/Densité |
| Densité linéaire + | Alt+Shift+A | Calque ou pinceau · priorité Éponge/Densité |
| Éclaircir | Alt+Shift+G | Calque ou pinceau · priorité Éponge/Densité |
| Écran | Alt+Shift+S | Calque ou pinceau · priorité Éponge/Densité |
| Densité couleur − | Alt+Shift+D | Calque ou pinceau · priorité Éponge/Densité |
| Densité linéaire − | Alt+Shift+W | Calque ou pinceau · priorité Éponge/Densité |
| Incrustation | Alt+Shift+O | Calque ou pinceau · priorité Éponge/Densité |
| Lumière tamisée | Alt+Shift+F | Calque ou pinceau · priorité Éponge/Densité |
| Lumière crue | Alt+Shift+H | Calque ou pinceau · priorité Éponge/Densité |
| Lumière vive | Alt+Shift+V | Calque ou pinceau · priorité Éponge/Densité |
| Lumière linéaire | Alt+Shift+J | Calque ou pinceau · priorité Éponge/Densité |
| Lumière ponctuelle | Alt+Shift+Z | Calque ou pinceau · priorité Éponge/Densité |
| Mélange maximal | Alt+Shift+L | Calque ou pinceau · priorité Éponge/Densité |
| Différence | Alt+Shift+E | Calque ou pinceau · priorité Éponge/Densité |
| Exclusion | Alt+Shift+X | Calque ou pinceau · priorité Éponge/Densité |
| Teinte | Alt+Shift+U | Calque ou pinceau · priorité Éponge/Densité |
| Saturation | Alt+Shift+T | Calque ou pinceau · priorité Éponge/Densité |
| Couleur | Alt+Shift+C | Calque ou pinceau · priorité Éponge/Densité |
| Luminosité | Alt+Shift+Y | Calque ou pinceau · priorité Éponge/Densité |
| Préférences (raccourci Adobe) | Ctrl+K | Document |
| Défiler vers le haut de dix unités | Shift+PgUp | Canevas |
| Défiler vers le bas de dix unités | Shift+PgDown | Canevas |
| Défiler à gauche de dix unités | Ctrl+Shift+PgUp | Canevas |
| Défiler à droite de dix unités | Ctrl+Shift+PgDown | Canevas |

## Gestes existants recensés

| Geste | Touches ou souris | Contexte |
| --- | --- | --- |
| Désaturer | Alt+Shift+D | Éponge · même touche que le mode de fusion |
| Saturer | Alt+Shift+S | Éponge · même touche que le mode de fusion |
| Cibler les ombres | Alt+Shift+S | Densité +/− · même touche que le mode de fusion |
| Cibler les tons moyens | Alt+Shift+M | Densité +/− · même touche que le mode de fusion |
| Cibler les hautes lumières | Alt+Shift+H | Densité +/− · même touche que le mode de fusion |
| Parcourir les outils d’un groupe | Maj + raccourci du groupe | Barre d’outils |
| Parcourir les outils masqués | Alt + clic sur l’outil | Barre d’outils |
| Activer temporairement un outil | Maintenir son raccourci | Selon le réglage des outils temporaires |
| Navigation temporaire | Espace + glisser | Canevas |
| Zoom avant temporaire | Ctrl + Espace + clic | Canevas |
| Zoom arrière temporaire | Alt + Espace + clic | Canevas |
| Opacité / intensité / exposition | 0–9, deux chiffres rapprochés | Peinture · 0 = 100 %, 45 = 45 % |
| Débit | Maj + 0–9 | Outils de peinture |
| Curseur précis | Verr. Maj | Canevas |
| Tracer une ligne entre deux points | Maj + clic | Pinceau |
| Prélever une couleur temporairement | Alt + clic | Pinceau, gomme, dégradé et remplissage |
| Prélever la couleur d’arrière-plan | Alt + clic | Pipette |
| Ajouter un échantillon | Maj + clic | Pipette |
| Supprimer un échantillon | Alt + clic | Échantillonnage de couleur |
| Taille / dureté avec la souris | Ctrl + Alt + glisser (Mac) ; Alt + clic droit + glisser (Windows) | Peinture |
| Ajouter à la sélection | Maj + glisser | Outils de sélection |
| Soustraire de la sélection | Alt + glisser | Outils de sélection |
| Intersection de sélections | Maj + Alt + glisser | Outils de sélection |
| Contraindre les proportions / angles | Maj + glisser | Sélection, formes et recadrage |
| Dessiner depuis le centre | Alt + glisser | Sélection et formes |
| Déplacer le tracé pendant le geste | Espace + glisser | Sélection et formes |
| Déplacer d’un pixel | Flèches | Déplacement ou sélection |
| Déplacer de dix pixels | Maj + flèches | Déplacement ou sélection |
| Déplacer le contenu sélectionné | Ctrl + flèches | Outils de sélection |
| Déplacer une copie | Alt + glisser | Déplacement |
| Définir la source de copie | Alt + clic | Tampon / correcteur |
| Valider le recadrage | Entrée | Recadrage |
| Annuler le geste en cours | Échap | Canevas |
| Retirer le dernier sommet | Retour arrière | Lasso polygonal |
| Fermer la sélection polygonale | Entrée / double-clic | Lasso polygonal |
| Afficher le masque seul | Alt + clic sur la vignette | Masque de fusion |
| Activer/désactiver le masque | Maj + clic sur la vignette | Masque de fusion |
| Isoler le calque | Alt + clic sur l’œil | Calques |
| Renommer le calque | Double-clic sur le nom | Calques |
| Humidité du pinceau mélangeur | 0–9 | Pinceau mélangeur |
| Mélange du pinceau mélangeur | Alt + Maj + 0–9 | Pinceau mélangeur |
| Opacité du calque | 0–9 | Déplacement |
| Mettre humidité et mélange à zéro | 00 | Pinceau mélangeur |

## Inventaire NYI

Ces entrées constituent la liste de travail des fonctions manquantes, sans annoncer qu’elles sont implémentées.

| Fonction manquante | Référence clavier ou souris | Contexte |
| --- | --- | --- |
| Couper les pixels du calque | Ctrl+X | Document |
| Copier les pixels du calque | Ctrl+C | Document |
| Coller sur place | Ctrl+Shift+V | Document |
| Coller dans la sélection | Ctrl+Alt+Shift+V | Document |
| Atténuer la dernière opération | Ctrl+Shift+F | Document |
| Transformation manuelle | Ctrl+T | Document |
| Répéter la transformation | Ctrl+Shift+T | Document |
| Transformer une copie | Ctrl+Alt+T | Document |
| Répéter avec une copie | Ctrl+Alt+Shift+T | Document |
| Déformation | Aucun raccourci par défaut | Transformation manuelle |
| Sélectionner et masquer | Ctrl+Alt+R | Document |
| Mode masque rapide | Q | Document |
| Transformer la sélection | Aucun raccourci par défaut | Document |
| Fusionner les calques sélectionnés | Ctrl+E | Document |
| Fusionner les calques visibles | Ctrl+Shift+E | Document |
| Créer un calque des visibles fusionnés | Ctrl+Alt+Shift+E | Document |
| Placer le calque tout en haut | Ctrl+Shift+] | Document |
| Placer le calque tout en bas | Ctrl+Shift+[ | Document |
| Nouveau calque par copie de sélection | Ctrl+J | Document |
| Nouveau calque par couper | Ctrl+Shift+J | Document |
| Verrouiller les pixels transparents | / | Document |
| Taille de la zone de travail | Ctrl+Alt+C | Document |
| Niveaux (réglage direct) | Ctrl+L | Document |
| Courbes (réglage direct) | Ctrl+M | Document |
| Teinte/Saturation (réglage direct) | Ctrl+U | Document |
| Balance des couleurs (réglage direct) | Ctrl+B | Document |
| Noir et blanc (réglage direct) | Ctrl+Alt+Shift+B | Document |
| Tonalité automatique | Ctrl+Shift+L | Document |
| Contraste automatique | Ctrl+Alt+Shift+L | Document |
| Couleur automatique | Ctrl+Shift+B | Document |
| Désaturer | Ctrl+Shift+U | Document |
| Changer le mode d’écran | F | Document |
| Mode d’écran précédent | Shift+F | Document |
| Afficher/masquer les extras | Ctrl+H | Document |
| Afficher/masquer la grille | Ctrl+' | Document |
| Afficher/masquer les repères | Ctrl+; | Document |
| Magnétisme | Ctrl+Shift+; | Document |
| Panneau Actions | F9 | Document |
| Couleurs d’épreuve | Ctrl+Y | Document |
| Couleurs non imprimables | Ctrl+Shift+Y | Document |
| Charger Rouge en sélection | Ctrl+Alt+3 | Document |
| Charger Vert en sélection | Ctrl+Alt+4 | Document |
| Charger Bleu en sélection | Ctrl+Alt+5 | Document |
| Remplir depuis l’historique | Ctrl+Alt+Backspace | Document |
| Préréglage de pinceau précédent | , | Document |
| Préréglage de pinceau suivant | . | Document |
| Premier préréglage de pinceau | Shift+, | Document |
| Dernier préréglage de pinceau | Shift+. | Document |
| Activer l’aérographe | Alt+Shift+P | Document |
| Arrière (pinceau) | Alt+Shift+Q | Pinceau |
| Effacer (pinceau) | Alt+Shift+R | Pinceau |
| Sélection de tracé et sélection directe | A | Document |
| Cadre | K | Document |
| Adapter à la fenêtre | Double-clic sur Main | Barre d’outils |
| Taille réelle | Double-clic sur Zoom | Barre d’outils |
| Superposition rouge du masque | \ | Masque sélectionné |
| Charger la transparence en sélection | Ctrl + clic sur la vignette | Calques |
| Charger le masque en sélection | Ctrl + clic sur la vignette | Masque |
| Ajouter le masque à la sélection | Ctrl + Maj + clic | Masque |
| Soustraire le masque | Ctrl + Alt + clic | Masque |
| Intersection avec le masque | Ctrl + Maj + Alt + clic | Masque |
| Décaler la source | Alt + Maj + flèches | Tampon |
| Tourner la source | Alt + Maj + < / > | Tampon |
| Redimensionner la source | Alt + Maj + [ / ] | Tampon |
| Effacer temporairement | Maintenir ` | Pinceau |
| Sélecteur de couleur sous le pointeur | Ctrl + Alt + Commande + glisser (Mac) | Peinture |
| Vue d’ensemble temporaire | Maintenir H + clic | Navigation |
| Afficher/masquer le voile de recadrage | / | Recadrage |
| Rapporteur | Alt + glisser l’extrémité | Règle |
| Point suivant de la courbe | + | Courbes |
| Point précédent de la courbe | − | Courbes |
| Déplacer le point de 1 / 10 | Flèches / Maj + flèches | Courbes |
| Supprimer le point choisi | Suppr | Courbes |
| Ajouter un point depuis l’image | Ctrl + clic | Courbes |
| Point sur chaque courbe de canal | Ctrl + Maj + clic | Courbes |
| Changer la grille | Alt + clic | Courbes |
| Afficher l’écrêtage | Alt + glisser | Courbes / Niveaux |
| Régler la famille de couleur sur l’image | Glisser sur l’image | Noir et blanc |
| Sélectionner tout le texte | Ctrl + A | Texte |
| Valider le texte | Ctrl + Entrée | Texte |
| Taille du texte | Ctrl + Maj + < / > | Texte |
| Interligne | Alt + flèches haut / bas | Texte |
| Approche du texte | Alt + flèches gauche / droite | Texte |
| Aligner le texte | Ctrl + Maj + L / C / R | Texte |
| Document suivant / précédent | Ctrl + Tab / Ctrl + Maj + Tab | Documents multiples |
| Écrêtage des ombres / hautes lumières | U / O | Développement RAW |
| Notation d’image | Ctrl + 1–5 | Développement RAW |
| Imprimer | Ctrl+P | Document |
| Informations du fichier | Ctrl+Alt+Shift+I | Document |
| Rétablir le fichier enregistré | F12 | Document |
| Répéter le dernier filtre | Ctrl+Alt+F | Document |
| Fluidité | Ctrl+Shift+X | Document |
| Filtre Camera Raw | Ctrl+Shift+A | Document |
| Enregistrer pour le web | Ctrl+Alt+Shift+S | Document |
| Plan de travail | V | Groupe d’outils Adobe |
| Pinceau de sélection | L | Groupe d’outils Adobe |
| Lasso magnétique | L | Groupe d’outils Adobe |
| Sélection d’objet | W | Groupe d’outils Adobe |
| Annotation | I | Groupe d’outils Adobe |
| Comptage | I | Groupe d’outils Adobe |
| Gomme d’arrière-plan | E | Groupe d’outils Adobe |
| Pinceau de réglage | Aucun raccourci par défaut | Groupe d’outils Adobe |
| Plume libre | P | Groupe d’outils Adobe |
| Plume de courbure | P | Groupe d’outils Adobe |
| Texte vertical | T | Groupe d’outils Adobe |
| Masque de texte vertical | T | Groupe d’outils Adobe |
| Masque de texte horizontal | T | Groupe d’outils Adobe |
| Sélection directe | A | Groupe d’outils Adobe |
| Forme personnalisée | U | Groupe d’outils Adobe |
| Masquer aussi la barre supérieure des options | Tab | Masquage intégral des panneaux Photoshop |
| Dialogue Remplir complet (motif, historique, contenu) | Shift+Backspace | Le sélecteur de couleur PhotoCrap est déjà disponible |

## Plume disponible depuis v0.13.1

P active le groupe Plume ; les outils Ajout/Suppression de point et Conversion de point sont accessibles par son clic droit. Les réglages répertorient aussi les gestes contextuels : Commande/Ctrl-glisser pour déplacer, Option/Alt pour convertir ou séparer une poignée, Maj pour contraindre, Retour/Échap pour terminer, clic sur l’origine pour fermer, Commande/Ctrl-Retour pour créer une sélection et Retour arrière/Suppr pour supprimer les points. Voir [les interactions détaillées](TOOL_INTERACTIONS.md#plume-et-tracés-bézier).

Le catalogue de v0.14 contient 298 entrées, dont 121 NYI. Les tracés de la Plume et les outils d’édition de points sont disponibles ; Plume libre, Plume de courbure, Sélection de tracé et Sélection directe restent NYI.

## Commandes de sélection ajoutées dans v0.14

Frange, Lisser, Dilater, Contracter, Sélectionner les calques similaires et Afficher/masquer le contour de sélection sont configurables, sans combinaison imposée par défaut. Resélectionner (Commande/Ctrl+Maj+D) et Intervertir (Commande/Ctrl+Maj+I) passent par les actions du menu, même lorsqu’il est fermé.

Le catalogue recense aussi les entrées NYI Étendre, Généraliser, Zone de mise au point, Sujet, Ciel, Mémoriser/Récupérer la sélection, Désélectionner les calques, Rechercher et Isoler les calques dans le panneau. Plage de couleurs est maintenant configurable (sans combinaison par défaut) ; Contour progressif utilise Maj+F6 et devient configurable. Sélectionner et masquer, Mode Masque et Transformer la sélection restent NYI. L’inventaire ne réserve pas les touches de ces fonctions manquantes. Voir [SELECTIONS.md](SELECTIONS.md) pour leurs options et les prérequis du moteur.
