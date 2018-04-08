# Documentation

## Authors
* *Gianmarco Cariggi* [@giacar](https://github.com/giacar)
* *Riccardo Ceccarelli* [@ricciricciard](https://github.com/ricciricciard)
* *Francesco Fabbi* [@Krelian96](https://github.com/Krelian96)

## Client
### Login:
* Client inserisce username e password appena si connette: *done*
* se utente non esiste allora i dati che ha inserito vengo usati per 
 registrare l'utente. *done*
* Se utente esiste, ma password non è corretta: server invia un 
 messaggio di errore al client. Si apre un while nel quale il client
 può inserire la password corretta. *done*
* Se utente esiste e password corretta, allora invia al server una 
 richiesta di tutti i dati della precedente sessione e il server 
 risponde con tali dati. *done*

### Comunicazione TCP:
* Al momento del login il server manda sul thread TCP ImagePackets
 contenenti id e texture di tutti i client presenti nel mondo e quelli che 
 arriveranno nel mondo. Quando il campo texture dell'ImagePacket è settato a
 NULL, vuol dire che quell'utente si è disconnesso e deve essere eliminato dal 
 mondo del client. *done*
* Inizializzazione client *done*
* Come fare per disconnettersi: ad intervalli regolari, il client sul thread
 TCP chiede la lista degli utenti connessi. Se vede la presenza di un nuovo 
 utente, o nuovi utenti, chiede al server le loro texture e le aggiunge al 
 mondo. Se vede che nella lista non ci sono persone che sono presenti nel suo
 mondo (cioè nella sua lista), allora le rimuove dal suo mondo. *done*

### Comunicazione UDP:
* Client, via UDP invia dei pacchetti VehicleUpdate al server. Il contenuto del 
 VehicleUpdate viene prelevato da desired_force, è contenuto nella struttura del 
 veicolo e si mette in attesa di pacchetti WorldUpdate che contengono al
 loro interno una lista collegata.Ricevuto questi pacchetti, smonta la lista 
 collegata all'interno e per ogni elemento della lista, preso l'id, preleva dal 
 mondo il veicolo con quell'id e ne aggiorna lo stato. I pacchetti di veicoli 
 ancora non aggiunti al proprio mondo vengono ignorati (per necessità). *done*

## Server
### Generale:
* Server avrà thread principale che accetta connessioni TCP, un thread per 
 client dove comunica in maniera TCP, e un thread UDP dove riceve tutti gli 
 update per ogni client e li aggiunge a una lista di VehicleUpdates. Ad 
 intervalli regolari, inserisce nei veicoli presenti nel suo mondo, tutte le
 forze prelevate dai VehicleUpdates e le aggiorna ai veicoli che c'ha. Se lui 
 riceve dati di client che ancora non sono presenti nel suo mondo, allora li 
 ignora. Successivamente, aggiorna il mondo (WorldUpdate), aggiornato il mondo
 prende tutte le posizioni dei veicoli che sono nel suo mondo, le inserisce in
 una lista di ClientUpdate che mette in un pacchetto di WorldUpdate e lo invia 
 a tutti i client che ha nella lista dei client connessi. *todo*
* Quando riceve la richiesta di tutti gli utenti connessi, restituisce tutti 
 gli utenti connessi. Quando riceve un pacchetto in cui è specificato un id e 
 una GetTexture, restituisce la texture relativa a quell'id. *todo*
* Quando il server riceve una stringa con su scritto "quit", allora lo rimuove 
 dalla lista dei client connessi. *todo*