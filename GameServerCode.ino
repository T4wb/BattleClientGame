/*
 * GameServer
 * Auteur: Tawwab Djalielie
 * 
 * ---------- COMMAND TABLE ----------
 *  1   =   "Select Opponent"
 *  2   =   "You must fight"    - Wordt niet meer gebruikt
 *  
 *  3   =   "Attack Mode"
 *  4   =   "Defence Mode"
 *  
 *  5   =   "Move 1"
 *  6   =   "Move 2"
 *  7   =   "Move 3"
 *  
 *  8   =   "Winner
 */

#include <LiquidCrystal.h>
#include <Wire.h>

// Defines
#define SELECT_OPPONENT 1
#define ATTACK_MODE 3
#define DEFENCE_MODE 4
#define WINNER 8

// Constants
// uint8_t; values: 0 - 255, 'u' geeft aan dat het getal unsigned is (geen conversie nodig)
const uint8_t MaximumPlayers = 4u;
const uint8_t MaximumConnectionID = 128u;
const uint8_t btn_StartGame = 2u;

// Variables

// Arrays
int ConnectedPlayers[MaximumPlayers];           // Spelers die zijn verbonden, de ID's
uint8_t PlayerStats[MaximumPlayers][5] = {{0}}; // De stats van de spelers
uint8_t BattlingPlayers[2] = {0, 0};            // De spelers die nu aan het vechten zijn
uint8_t BattlingModes[2];                       // De modes van de spelers die nu aan het vechten zijn, een 'A' voor attack mode, een 'D' voor defence mode
uint8_t MaxHealthPoints[2];                     // De max health van een speler die gaat vechten, zodat het na het gevecht kan worden gereset

// Server mode (enum)
typedef enum {Connecting, Setup, PrepareBattle, Battle} ServerMode;
ServerMode CurrentMode = Connecting;

//Zet de pinnen voor het lcd scherm
LiquidCrystal LCD(6, 7, 8, 9, 10, 11);

void setup() {
  // Clear de array zodat er nieuwe id's in kunnen worden gezet
  emptyConnectedPlayersArray();

  LCD.begin(16, 2);
  setLCDText("No connections..", "");
  
  // Begin communicatie
  Wire.begin();
  Serial.begin(9600);
  Serial.println("Starting Server \nSet SeverMode to Connecting");

  randomSeed(analogRead(1));
}

void loop() {
  switch (CurrentMode) {
    case Connecting: {
      /*
       * Deze case loopt van id = 0 naar id = 127.
       * De GameServer probeert verbinding te maken met het id.
       * Als er een 'antwoord' terug komt dan is het een arduino
       * die klaar is om te vechten.
       * Daarna wordt deze id on 'ConnectedPlayers' opgeslagen,
       * zodat de GameServer weet dat op dit id een speler zit.
       * Als er minimaal 2 spelers zijn en [maximaal 4 of er wordt op de knop]
       * gedrukt dan gaat het spel beginnen.
       */
       
      // Check of er connecties zijn gemaakt
      for (int id = 0; id < MaximumConnectionID; id++) {
        Wire.beginTransmission(id);
        if (Wire.endTransmission() == 0) {
          
          // Er is een connectie gevonden
          // Add de connectie bij de lijst
          bool added = false;
          for (int index = 0; index < MaximumPlayers && !added; index++) {
            // Kijk of de id al in de list zit
            if (ConnectedPlayers[index] == id) {
              added = true;
              
            } else if (ConnectedPlayers[index] == -1) {
              ConnectedPlayers[index] = id;
              added = true;
              showConnectedPlayersLCD();
              Serial.print("GameServer connected with ID: ");
              Serial.println(id);
            }
          }
        }
      }

      // Kijk of alle benodigde connecties zijn gemaakt
      // Als de 2e item in de lijst een valid is is (niet -1) dan is het min aantal spelers bereikt (1v1)
      // Dan moet er worden gekeken of de maximum aantal spelers is bereikt of dat er op de knop is gedrukt
      if (ConnectedPlayers[1] != -1 && (ConnectedPlayers[MaximumPlayers - 1] != -1 || digitalRead(btn_StartGame))) {
        CurrentMode = Setup;
        setLCDText("Waiting for","player stats...");
        Serial.println("Set ServerMode to Setup");
      }
    }
    break;
      
    case Setup: {
      /*
       * Deze case blijft vragen aan de spelers of ze nieuwe stats klaar hebben staan.
       * Als er nieuwe stats klaar staan worden deze van de spelers geupdate.
       * Als de stats zijn gestuurd worden de stats niet overschreven door een 0.
       * Als van iedere connected speler de stats zijn ontvangen dan gaat de 
       * GameServer door naar de volgende mode.
       */
       
      // Spelers sturen dit door: ATK MAG DEF HP
      // De GameClients sturen voor de setup altijd 4 bytes door met hun stats
      for (int i = 0; i < MaximumPlayers; i++) {
        int8_t id = ConnectedPlayers[i];

        // Als er niet een maximum aan spelers is geconnect zijn de overige id's in de lijst -1
        if (id != -1) {
          PlayerStats[i][0] = id;
          int stat_index = 1;
          int points = 19;
          
          Wire.requestFrom(id, 4);
          while (Wire.available()) {
            uint8_t c = Wire.read();
            // Als c 255 is is er "niks" doorgestuurd
            // 255 is ook nooit een valid waarde voor een stat
            // En als er 0 binnen krijgen willen we de ingevulde waarde niet overschrijven
            if (c != 255 && c != 0) {
              // checkStats kijkt of een speler heeft valsgespeeld
              // Als een speler heeft valsgespeeld mag die speler deze ronde niet meer mee spelen
              points = checkStats(id, points, stat_index, c);
              PlayerStats[i][stat_index] = c;
              stat_index++;
            }
          }
        }
      }

      // Kijk of alles is ingevuld, als er ergens een 0 is, is niet iedere speler klaar
      // j gaat tot 5 omdat iedere stats listuit het volgende bestaat:
      // id, attack, magic, hitpoints, defence
      // j moet dus omhoog worden gedaan als er nieuwe stats bij komen
      bool not_ready = false;
      for (int i = 0; i < MaximumPlayers && !not_ready; i++){
        for (int j = 0; j < 5 && !not_ready; j++) {
          // Als er een stat 0 is en er wordt gekeken of die lijst waar een 0 is een id heeft
          // Als die lijst een id heeft dan hoort opdezelfde plaats in de ConnectedPlayers Array hetzelfde id te staan
          // Als de speler er uit ligt is het id -1 en doen die stats er niet meer toe
          if (PlayerStats[i][j] == 0 && PlayerStats[i][0] == ConnectedPlayers[i]) {
            not_ready = true;
          }
        }
      }

      // Als not_ready false is dan is zijn alle stats ingevuld
      if (!not_ready) {
        setLCDText("Starting Battle!", "");
        CurrentMode = PrepareBattle;
        Serial.println("Set ServerMode to PrepareBattle");
      }
    }
    break;
      
    case PrepareBattle: {
      /*
       * Er wordt eerste 'random' een speler gekozen die aan het gevecht mag deelnemen.
       * 1. Als er meer dan 2 active players zijn dan mag deze speler zelf een
       * tegenstander kiezen.
       * De id's van de tegenstander worden dan naar de clients gestuurd.
       * En de id's worden op de LCD van de GameServer gezet.
       * De client stuurt een id terug van wat zijn tegenstander moet zijn.
       * 2. Als er maar 2 spelers over zijn dan wordt de laatst overgebleven
       * speler vanzelf gekozen als tegenstander.
       * 
       * Als de spelers zijn geselecteerd dan wordt er random een aanvaller gekozen.
       * De andere speler wordt dan vanzelf de verdediger.
       * Hierna wordt de max health van de spelers opgeslagen zodat deze na
       * het gevecht weer gereset kunnen worden.
       * 
       * Vervolgens krijgen de spelers de stats van de tegenstanders te zien.
       * Als alles is prepared dan worden de spelers op de hoogte gebracht van wat hun 
       * battle mode is (aanvaller of verdediger).
       */
       
      uint8_t start_player = 0;     // De speler die random wordt geselecteerd om te beginnen
      bool player_selected = false; // Is true als er een random speler is gekozen
      int maximum_tries = 123;      // Zodat er altijd een speler wordt gekozen, zodat er met een slechte random seed niet te lang wordt gewacht
      int index = 0;                // index die bijhoud bij welke speler die nu moet "rollen" voor een random getal
      while (!player_selected) {
        if (index >= MaximumPlayers) {
          index = 0;
        }

        // Kijk of de huidige speler nog in het spel is
        // Rol hierbij een random getal
        // Als het random getal een '0' is, is dit de speler die het spel gaat beginnen
        // Als maximum_tries is bereikt dan wordt er vanzelf al een speler gekozen
        if (ConnectedPlayers[index] != -1 && (random(MaximumPlayers) == 0 || maximum_tries <= 0)) {
          start_player = ConnectedPlayers[index];
          player_selected = true;
          Serial.print("Player selected to start battle: ");
          Serial.println(start_player);
        }

        index++;
        maximum_tries--;
      }

      uint8_t opponent;
      // Als er meer dan 2 spelers nog in het spel zitten, dan moet de "start" speler zijn tegenstander uitkiezen
      if (activePlayers() > 2) {
        noticeClient(start_player, SELECT_OPPONENT);
        setLCDSelectionMenu(start_player);
        delay(1000); // Een delay zodat de speler tijd heeft om te kiezen
        opponent = getOpponentFromClient(start_player);

      // Als er nog maar 2 spelers over zijn dan is de tegenstander al vanzelf geselecteerd, namelijk de laatst over gebleven speler
      } else {
        for (int i = 0; i < MaximumPlayers; i++) {
          if (ConnectedPlayers[i] != -1 && ConnectedPlayers[i] != start_player) {
            opponent = ConnectedPlayers[i];
          }
        }
      }
      
      Serial.print("Selected opponent is: ");
      Serial.println(opponent);
        
      BattlingPlayers[0] = start_player;  // Player id #1
      BattlingPlayers[1] = opponent;      // Player id #2

      // Selecteer random een speler om te beginnen
      int8_t first_player = random(2);
      int8_t second_player = 1 - first_player;
      BattlingModes[first_player] = ATTACK_MODE;
      BattlingModes[second_player] = DEFENCE_MODE;

      // Get de maximale hp van  de spelers, voor de reset na het gevecht
      for (int i = 0; i < 2; i++) {
        bool done = false;
        for (int j = 0; j < MaximumPlayers && !done; j++) {
          if (BattlingPlayers[i] == PlayerStats[j][0]) {
            MaxHealthPoints[i] = PlayerStats[j][4];
            done = true;
          }
        }
      }

      showStatsPlayers();

      // Zeg tegen de clients welke mode ze nu zijn/worden
      for (int i = 0; i < 2; i++) {
        noticeClient(BattlingPlayers[i], BattlingModes[i]);
      }

      Serial.print("Attacker: ");
      Serial.println(BattlingPlayers[first_player]);
      Serial.print("Defender: ");
      Serial.println(BattlingPlayers[second_player]);

      setLCDText((String)BattlingPlayers[0] + " vs " + (String)BattlingPlayers[1], "");
      CurrentMode = Battle;
      Serial.println("Set ServerMode to Battle");
    }
    break;
      
    case Battle: {
      /*
       * Hier wordt eerst de move van de spelers opgehaald.
       * De spelers turen een code van 5-7 terug, dit geeft aan welke
       * aanval of verdediging er wordt gedaan.
       * Daarna wordt de 'damage' berekent van de move combinatie.
       * Als de damage negatief is, is het een reflect en damaga voor de attacker.
       * Als de damage positief is dan is het damage voor de defender.
       * 
       * Vervolgens worden de clients op de hoogte gesteld van wat hun nieuwe health is.
       * Hierna wordt er gekeken of er een speler 'af' is, door te kijken of zijn
       * health points 0 is.
       * 
       * Als die 0 is wordt er getoond wie de winnaar is, wordt de speler eruit gezet
       * en dan gaat de GameServer terug naar 'PrepareBattle'
       * 
       * Anders gebeurt er niks en worden de battle modes geswitched en gestuurd
       * naar de spelers.
       */
      
      // Kijk welke moves de spelers maken
      uint8_t move_attacker = getMove(ATTACK_MODE);
      uint8_t move_defender = getMove(DEFENCE_MODE);
      int8_t damage = calculateDamage(move_attacker, move_defender);

      Serial.print("Move attacker: ");
      Serial.println(move_attacker);
      Serial.print("Move defender: ");
      Serial.println(move_defender);

      // Schade voor de aanvaller
      if (damage < 0) {
        damage *= -1;
        damagePlayer(ATTACK_MODE, damage);
        setLCDText((String)damage + " damage dealt", "to the attacker!");
        Serial.print("Attacker received ");
        Serial.print(damage);
        Serial.println(" damage!");

      // Schade voor de verdediger
      } else if (damage > 0) {
        damagePlayer(DEFENCE_MODE, damage);
        setLCDText((String)damage + " damage dealt", "to the defender!");
        Serial.print("Defender received ");
        Serial.print(damage);
        Serial.println(" damage!");

      // Schade = 0
      } else {
        setLCDText("No (0) damage", "has been dealt.");
        Serial.print("Defender received ");
        Serial.print(damage);
        Serial.println(" damage!");
      }

      // Update spelers met nieuwe health
      updatePlayers();

      // Check of er een speler game over is
      bool game_over = false;
      for (int i = 0; i < MaximumPlayers && !game_over; i++) {
        for (int j = 0; j < 2 && !game_over; j++) {
          // Zoek voor de verliezer, er wordt gekeken of health <= 0
          if (BattlingPlayers[j] == PlayerStats[i][0] && PlayerStats[i][4] <= 0) {
            game_over = true;
            setLCDText("Winner is:", (String)BattlingPlayers[1 - j]);
            noticeClient(BattlingPlayers[1 - j], WINNER);
            removePlayer(BattlingPlayers[j]);
            resetHealth();
            Serial.print("Winner is: ");
            Serial.println(BattlingPlayers[1 - j]);

            // Als er nog minstens 2 spelers levend zijn
            if (activePlayers() >= 2) {
              Serial.println("Start new battle with remaining players");
              CurrentMode = PrepareBattle;

            // Anders heeft er iemand gewonnen
            } else {
              Serial.println("Reconnecting players...");
              resetStats();                 // Alle stats worden naar 0 gezet
              emptyConnectedPlayersArray(); // Alle connecties worden naar -1 gezet (default)
              CurrentMode = Connecting;
            }

            delay(5000); // Een delay van 5 sec, zodat mensen kunnen zien op de server wie de winner is
          }
        }
      }

      // Delay zodat de clients zich kunnen updaten
      delay(500);
      
      // Switch modes
      uint8_t temp = BattlingModes[0];
      BattlingModes[0] = BattlingModes[1];
      BattlingModes[1] = temp;

      // Stuur de nieuwe mode naar de speler, mits het niet game_over is
      for (int i = 0; i < 2 && !game_over; i++) {
        noticeClient(BattlingPlayers[i], BattlingModes[i]);
      }
      Serial.println("Players updated");
    }
    break;
      
    default:
      Serial.print("Invalid Server Mode");
      delay(10000);
      break;
  }
}


/*
 * Reset functies
 * Aantal: 4
 */
void emptyConnectedPlayersArray() {
  for (int i = 0; i < MaximumPlayers; i++) {
    ConnectedPlayers[i] = -1;
  }
}

void resetHealth() {
  bool done;
  for (int i = 0; i < 2; i++) {
    done = false;
    for (int j = 0; i < MaximumPlayers && !done; j++) {
      if (BattlingPlayers[i] == PlayerStats[j][0]){
        PlayerStats[j][4] = MaxHealthPoints[i];
        done = true;
      }
    }
  }
}

void resetStats() {
  for (int i = 0; i < MaximumPlayers; i++) {
    for (int j = 0; j < 5; j++) {
      PlayerStats[i][j] = 0;
    }
  }
}

void removePlayer(uint8_t id) {
  for (int i = 0; i < MaximumPlayers; i++) {
    if (ConnectedPlayers[i] == id) {
      ConnectedPlayers[i] = -1;
    }
  }
}


/*
 * Check functies
 * Aantal: 1
 */
int checkStats(uint8_t player_id, int points_left, int stat, uint8_t stat_value) {
  // stat 0 = player_id, die hoeft niet te wordenmeegenomen
  // stat 4 = Hitpoints en hitpoints heeft een waarde van 5 per point en met een base van 50
  if (stat == 0) {
    stat_value = 0;
    
  } else if (stat == 4) {
    stat_value = (stat_value - 50) / 5;
  }
  
  points_left -= stat_value;

  // De speler heeft valsgespeeld!
  if (points_left < 0) {
    removePlayer(player_id);
    Serial.print("Player removed from current game: ");
    Serial.println(player_id);
  }
  
  return points_left;
}


/*
 * LCD print functies
 * Aantal: 4
 */
void setLCDSelectionMenu(uint8_t filter_player) {
  LCD.clear();
  LCD.print("Select opponent");
  LCD.setCursor(0, 1);
  for (int i = 0; i < MaximumPlayers; i++) {
    if (ConnectedPlayers[i] != -1 && ConnectedPlayers[i] != filter_player) {
      LCD.print(ConnectedPlayers[i]);
      LCD.print(" ");
    }
  }
}

void showConnectedPlayersLCD() {
  String connected_players = "";
  for (int i = 0; i < MaximumPlayers; i++) {
    if (ConnectedPlayers[i] != -1){
      connected_players += (String)ConnectedPlayers[i] + " ";
    }
  }
  setLCDText("Connected with:", connected_players);
}

void setLCDText(String line_1, String line_2) {
  LCD.clear();
  LCD.print(line_1);
  LCD.setCursor(0, 1);
  LCD.print(line_2);
}


/*
 * Transmission functies
 * Aantal: 5
 */
void updatePlayers() {
  // Stuur de huidige health naar de spelers
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < MaximumPlayers; j++) {
      if (BattlingPlayers[i] == PlayerStats[j][0]) {
        Wire.beginTransmission(BattlingPlayers[i]);
        Wire.write(PlayerStats[j][4]);
        Wire.endTransmission();
      }
    }
  }
}

void noticeClient(uint8_t player, uint8_t command) {
  Wire.beginTransmission(player);
  Wire.write(command);
  Wire.endTransmission();
}

uint8_t getOpponentFromClient(uint8_t player) {
  // for loop om alle tegenstander id's te verzamelem
  uint8_t opponent_ids[MaximumPlayers - 1] = {0};
  int index = 0;
  for (int i = 0; i < MaximumPlayers; i++) {
    if (ConnectedPlayers[i] != player && ConnectedPlayers[i] != -1) {
      opponent_ids[index] = ConnectedPlayers[i];
      index++;
    }
  }

  // Stuur alle tegenstanders door naar de startende speler
  Wire.beginTransmission(player);
  Wire.write(opponent_ids, MaximumPlayers - 1);
  Wire.endTransmission();
  
  // Wacht totdat er een legit antwoord terug is van de speler
  bool got_response = false;
  while (!got_response) {
    Wire.requestFrom(player, 1);
    
    if (Wire.available()) {
      uint8_t opponent_id = Wire.read();
      
      if (opponent_id != 0 && opponent_id != player) {
        bool found = false;
        for (int i = 0; i < MaximumPlayers; i++) {
          if (ConnectedPlayers[i] == opponent_id) {
            found = true;
          }
        }
  
        // Een non-actieve speler heeft als id -1
        // Dus als er iets is misgegaan en het verkeerde id is genomen
        // Blijft het wachten op een correct antwoord
        if (opponent_id != -1 && opponent_id != player && found) {
          got_response = true;
          return opponent_id;
        }
      }
    }
  }
}

void showStatsPlayers() {
  for (int i = 0; i < 2; i++) {
    bool done = false;
    for (int j = 0; j < MaximumPlayers && !done; j++) {
      if (BattlingPlayers[1 - i] == PlayerStats[j][0]) { // 1- i, is de tegenstander
        Wire.beginTransmission(BattlingPlayers[i]);
        Wire.write(PlayerStats[j], 5); 
        Wire.endTransmission();
        done = true;
      }
    }
  }
}

uint8_t getMove(uint8_t mode) {
  // Deze functie verkijgt de move die een speler heeft gedaan
  for (int i = 0; i < 2; i++) {
    if (BattlingModes[i] == mode) {

      // Wacht todat er een goed antwoord is
      bool received = false;
      while (!received) {
        
        Wire.requestFrom(BattlingPlayers[i], 1);
        while (Wire.available()) {
          uint8_t turn_move = Wire.read();

          if (turn_move >= 5 && turn_move <= 7) {
            received = true;
            turn_move -= 4; // Maak de waarden tussen 1-3
            return turn_move;
          }
        }
      }
    }
  }
}


/*
 * Utility functies
 * Aantal: 1
 */
uint8_t activePlayers() {
  uint8_t active_players = 0;
  for (int i = 0; i < MaximumPlayers; i++) {
    if (ConnectedPlayers[i] != -1) {
      active_players++;
    }
  }
  return active_players;
}


/*
 * Battle functies
 * Aantal:3
 */
uint8_t getStats(uint8_t mode, uint8_t stat) {
  // Deze functie wordt gebruikt in calculateDamage
  for (int i = 0; i < MaximumPlayers; i++) {
    for (int j = 0; j < 2; j++) {
      if (BattlingModes[j] == mode && BattlingPlayers[j] == PlayerStats[i][0]) {
        return PlayerStats[i][stat];
      }
    }
  }
}


int8_t calculateDamage(uint8_t attack, uint8_t defence) {
  int8_t damage = 5; // Base damage
  switch (attack) {
    case 1: // "Attack"

      switch (defence) { 
        case 1: // "Defend"
          damage = damage + (1 * getStats(3, 1)) - getStats(4, 3);
          break;

        case 2: // "Magic Block"
          damage = damage + (2 * getStats(3, 1)) - getStats(4, 3);
          break;

        case 3: // "Counter"
          damage = damage + (1.5 * getStats(3, 1)) - getStats(4, 3);
          break;
      }
      
      break;

    case 2: // "Strike"

      switch (defence) { 
        case 1: // "Defend"
          damage = damage + (4 * getStats(3, 1)) - getStats(4, 3);
          break;

        case 2: // "Magic Block"
          damage = damage + (4 * getStats(3, 1)) - getStats(4, 3);
          break;

        case 3: // "Counter"
          damage = -(damage + (4 * getStats(3, 1)) - getStats(4, 3));
          break;
      }
      
      break;

    case 3: // "Magic"

      switch (defence) { 
        case 1: // "Defend"
          damage = damage + (2 * getStats(3, 2)) - getStats(4, 3);
          break;

        case 2: // "Magic Block"
          damage = 0;
          break;

        case 3: // "Counter"
          damage = damage + (2.5 * getStats(3, 2)) - getStats(4, 3);
          break;
      }
      
      break;
  }

  if (damage < 0 && attack != 2 && defence != 3) {
    damage = 0;
  }
  return damage;
}

void damagePlayer(uint8_t player_mode, int8_t damage) {
  for (int i = 0; i < 2; i++) {

    // Kijk of de juiste persoon de schade krijgt
    if (BattlingModes[i] == player_mode) {
      bool done = false;
      for (int j = 0; j < MaximumPlayers && !done; j++) {

        // Nog een check of het de juiste speler is
        if (BattlingPlayers[i] == PlayerStats[j][0]) {
          done = true;
          // Kijk of de damage groter is dan de health, want health is unsigned
          if (damage > PlayerStats[j][4]) {
            PlayerStats[j][4] = 0;
            
          } else {
            PlayerStats[j][4] -= damage;
          }
        }
      }
    }
  }
}

