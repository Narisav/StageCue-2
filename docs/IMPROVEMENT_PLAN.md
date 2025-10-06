# Pistes d'amélioration pour une version commerciale

Cette liste priorise les actions nécessaires pour rendre le système StageCue industrialisable et fiable en exploitation professionnelle.

## 1. Fiabilité du firmware
- **Éliminer les corruptions mémoire côté WebSocket.** L'appel `data[len] = 0` écrit hors du tampon fourni par `ESPAsyncWebServer`, ce qui peut provoquer un crash selon la taille réelle allouée. Il faut soit copier les données dans un tampon local, soit utiliser `String((char*)data).substring(0, len)` pour garantir la terminaison. 【F:firmware/src/web_server.cpp†L28-L47】
- **Confirmer la réception de messages invalides.** Valider la taille des trames, la présence des champs JSON et renvoyer une erreur (ou ignorer) pour les indices hors bornes afin d'éviter des états incohérents. 【F:firmware/src/web_server.cpp†L35-L47】
- **Éviter la fragmentation mémoire due à `String`.** Les chaînes dynamiques créées pour chaque cue peuvent fragmenter l'heap de l'ESP32. Remplacer par des buffers `char` de taille fixe ou `std::array<char, N>` pour chaque cue. 【F:firmware/src/cues.cpp†L9-L31】
- **Remettre les sorties à l'état bas et gérer la durée d'activation.** Le code force les LED à `HIGH` sans remise à zéro ni gestion de temporisation, ce qui peut bloquer le système en état actif permanent. Prévoir un timer ou un reset explicite. 【F:firmware/src/cues.cpp†L23-L31】
- **Debounce et supervision des boutons physiques.** `updateCues()` est vide; il doit gérer un anti-rebond matériel/logiciel, la répétition, la priorisation et la sécurité de déclenchement manuel. 【F:firmware/src/cues.cpp†L19-L21】
- **Éviter les blocages du bus I²C.** `initDisplay()` suppose trois écrans avec des adresses consécutives (`OLED_ADDRESS + i`), alors que des SSD1306 partagent la même adresse par défaut. Préciser/paramétrer les adresses ou intégrer un multiplexeur/driver alternatif. Ajouter une vérification de retour de `begin`. 【F:firmware/src/display_manager.cpp†L8-L29】
- **Gérer l’absence ou la panne d’un écran.** En cas d'échec `begin()`, le système devrait enregistrer l'erreur, empêcher les accès ultérieurs et alerter l'interface. 【F:firmware/src/display_manager.cpp†L14-L20】
- **Compléter la machine d'état d'initialisation.** `setup()` lance directement le point d'accès sans essayer de se reconnecter au Wi-Fi connu; il faut distinguer mode infrastructure, fallback AP, et repli sur un portail captif. 【F:firmware/src/stagecue.ino†L7-L17】【F:firmware/src/wifi_portal.cpp†L5-L19】
- **Implémenter réellement le portail captif.** Le front-end `wifi.html` attend des routes `/scan` et `/save_wifi` qui n'existent pas; il faut ajouter le serveur de configuration, la persistance (Preferences), et la reconnexion automatique. 【F:firmware/src/wifi_portal.cpp†L5-L19】【F:firmware/data/wifi.html†L34-L67】

## 2. Robustesse réseau et synchronisation
- **Synchroniser l’état lors des connexions WebSocket.** Les nouveaux clients ne reçoivent aucun état initial; envoyer la liste des cues actifs et leurs textes dès `WS_EVT_CONNECT`. 【F:firmware/src/web_server.cpp†L18-L47】
- **Éviter les boucles de notification.** `triggerCue()` renvoie `ws.textAll` alors que `onWebSocketEvent()` rappelle `triggerCue`, ce qui multiplie les diffusions. Prévoir un indicateur pour éviter les rebouclages et différencier les origines (local, API, WebSocket). 【F:firmware/src/web_server.cpp†L43-L47】【F:firmware/src/cues.cpp†L23-L31】
- **Ajouter un mécanisme d’accusé de réception côté navigateur.** Sans confirmation, l’interface n’a aucun retour sur la réussite d’un cue. Ajouter un message `ack` avec horodatage et résultat. 【F:firmware/data/app.js†L1-L33】
- **Sécuriser l’accès réseau.** Le point d’accès est ouvert avec un mot de passe faible et aucune authentification HTTP. Prévoir WPA2 personnalisé, certificats HTTPS (via ESP32), ou au minimum une clé forte et des rôles utilisateur. 【F:firmware/src/wifi_portal.cpp†L5-L19】【F:firmware/src/web_server.cpp†L62-L80】

## 3. UX & interface web
- **Précharger les textes existants.** Les champs restent vides au chargement alors que des textes par défaut existent; renvoyer l’état via une API REST ou WebSocket initial. 【F:firmware/data/index.html†L12-L31】【F:firmware/src/cues.cpp†L12-L31】
- **Empêcher l’envoi de chaînes vides.** `sendCue()` transmet même si `text` est vide, ce qui efface potentiellement un affichage. Ajouter une validation côté client et serveur. 【F:firmware/data/app.js†L19-L28】【F:firmware/src/web_server.cpp†L35-L47】
- **Gérer les erreurs réseau côté UI.** En cas d’échec de `fetch` ou de WebSocket, afficher un message utilisateur clair et proposer un mode dégradé. 【F:firmware/data/app.js†L1-L33】【F:firmware/data/wifi.html†L34-L67】
- **Internationalisation & accessibilité.** Offrir une interface multilingue, des libellés ARIA, une navigation clavier pour respecter les normes d’accessibilité. 【F:firmware/data/index.html†L1-L36】

## 4. Maintenance, logs et monitoring
- **Structurer les logs.** Les `Serial.printf` émojis sont pratiques en dev mais peu exploitables en production. Ajouter des niveaux de log, timestamps, et la possibilité de les expédier via syslog/MQTT. 【F:firmware/src/web_server.cpp†L18-L80】【F:firmware/src/wifi_portal.cpp†L5-L19】
- **Ajouter un watchdog logiciel.** En cas de blocage (I²C, Wi-Fi), un watchdog redémarrera l’appareil. À compléter par un mécanisme de health-check exposé sur HTTP. 【F:firmware/src/stagecue.ino†L7-L17】
- **Mettre en place des tests unitaires/simulation.** Créer une configuration PlatformIO avec tests sur le parsing JSON, la gestion d’état des cues et la logique de fallback réseau.

## 5. Industrialisation & sécurité
- **Séparer les secrets de compilation.** Ne jamais embarquer de SSID/mots de passe en dur; utiliser `secrets.h` ignoré par Git et un provisioning sécurisé. 【F:firmware/src/config.h†L7-L24】
- **Durcir le filesystem.** Vérifier l’intégrité de `LittleFS` au boot, prévoir un mécanisme de mise à jour OTA signé et de rollback en cas d’échec. 【F:firmware/src/web_server.cpp†L56-L80】
- **Protection physique.** Ajouter une option pour verrouiller les déclenchements matériels (interrupteur maître, clé ou code) afin d’éviter les activations accidentelles. 【F:firmware/src/cues.cpp†L12-L31】
- **Documentation & support.** Rédiger un manuel utilisateur, un guide d’installation, et prévoir un système de tickets.

## 6. Roadmap recommandée
1. Sécuriser le traitement WebSocket, la persistance et la configuration Wi-Fi.
2. Ajouter la gestion complète des cues (statuts, timers, watchdog, resets).
3. Implémenter la configuration réseau (scan, sauvegarde Preferences, captive portal).
4. Industrialiser l’UI (état initial, validation, messages d’erreur, accessibilité).
5. Mettre en place l’infrastructure de déploiement (tests, CI, OTA, documentation).
