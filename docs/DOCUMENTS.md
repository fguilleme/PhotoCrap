# Documents multiples

Chaque document possède son onglet au-dessus du canevas, ses calques, ses masques,
sélections, tracés, historique et rendus en arrière-plan. Le zoom, le défilement
et la rotation de la vue sont conservés lors des changements d’onglet.

## Ouvrir et créer

**Fichier → Ouvrir**, les fichiers récents et les fichiers transmis par le système
ouvrent un document distinct. Déposer une ou plusieurs images dans la zone
principale ou l’accueil ouvre un onglet par fichier. Un fichier déjà ouvert est
activé au lieu d’être chargé une deuxième fois. Le document vierge initial peut
être réutilisé pour la première ouverture ; les autres documents sont conservés.
**Nouveau…** crée un onglet indépendant. Les images RAW conservent leur développement
asynchrone et la précision 16 bits.

Le dépôt dans la pile des calques et **+ → Importer une image…** continuent à
importer dans le document actif, sans créer d’onglet.

## Naviguer, enregistrer et fermer

Cliquer un onglet affiche son document et rétablit sa vue. Le point indique des
modifications non enregistrées ; une activité ou un résultat IA en attente est
également signalé dans l’onglet. **Enregistrer** et **Exporter** concernent uniquement
le document actif. Les raccourcis **Fermer le document**, **Document suivant** et
**Document précédent** sont modifiables dans **Réglages → Raccourcis clavier**.

La croix de l’onglet ou **Fichier → Fermer le document** demande quoi faire des
modifications. Une demande IA ou un résultat en attente demande aussi confirmation ;
fermer ce document annule sa demande. Fermer le dernier onglet laisse un document
vierge disponible.

**Quitter** vérifie tous les documents, y compris ceux qui ne sont pas affichés.
Enregistrer, abandonner ou annuler est proposé pour chaque document concerné.
Annuler laisse tous les onglets ouverts ; aucun n’est détruit pendant cette revue.
Les ouvertures transmises par le système pendant une saisie modale ou une sauvegarde
sont mises en attente, puis traitées lorsque l’interface redevient disponible.

## Résultat IA dans un nouveau document

Dans **Créer avec l’IA… → Destination**, choisir :

- **Nouveau calque dans ce document** : le résultat retrouve son placement et les
  dimensions du document source, comme auparavant.
- **Nouveau document · résolution native** : le résultat conserve les dimensions
  renvoyées par le fournisseur, sans remise à l’échelle vers l’ancien canevas.
  Un document non enregistré est créé avec un calque, les informations IA et
  le masque éventuel remis à l’échelle. Les pixels et l’historique de la source
  restent intacts.

Topaz et Real-ESRGAN proposent le nouveau document par défaut. Les autres profils
proposent le nouveau calque, tout en permettant de choisir le nouveau document.
La source IA originale et ses petites dimensions restent disponibles pour éviter
d’envoyer une version déjà agrandie avant l’upscale.

La demande reste attachée à son document d’origine lorsqu’on change d’onglet.
Si elle termine alors qu’un autre onglet est actif, le résultat est ajouté sans
voler le focus. Une édition ou un dialogue en cours empêche également ce changement
automatique de document. Fermer le document source annule le traitement associé.

Les limites IA restent **32 Mpx et 16 384 px par côté**. Le document résultant
utilise la profondeur de travail 8 ou 16 bits de sa source ; une réponse 8 bits
convertie en document 16 bits n’acquiert pas de précision supplémentaire.
Le format des projets `.pcr` reste inchangé et chaque fichier contient un document.
