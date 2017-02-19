/*
   GameClient
   Auteur: Tawwab Djalielie
   Klas: ICT 1
   Vak: PIT 2

   ---------- COMMAND TABLE ----------
    1   =   "Select Opponent"

    3   =   "Attack Mode"
    4   =   "Defence Mode"

    5   =   "Move 1"
    6   =   "Move 2"
    7   =   "Move 3"

    8   =   "Winner
*/

#include <LiquidCrystal.h>
#include <Wire.h>

//Command table geïmplementeerd
//Ontvangst berichten
#define CHOOSE_OPP 1
#define ATTACKER 3
#define DEFENDER 4
#define WINNER 8

//Verzend codes
#define NO_MESSAGE 0
#define MOVE_1 5
#define MOVE_2 6
#define MOVE_3 7



//Variabelen

const int id = 77; //Client ID, dit moet uniek zijn voor elke client

typedef enum {Setup, Standby, SelectOpponent, Attack, Defend, ReceiveHP, Death, NextBattle} ClientMode; //Client Mode
ClientMode CurrentMode = Setup;
ClientMode NextMode = NextBattle;

//Knoppen
const int button_action1 = 7;
const int button_action2 = 6;
const int button_action3 = 5;
const int button_confirm = 4;
unsigned long changeTime;     //Wordt gebruikt om tijd bij te houden als op een knop is gedrukt


const String lcd_positions[4] = {"ATK: ", "MAG: ", "DEF: ", "HP: "}; //Text string om stats weer te geven
uint8_t stats[4] = {1, 1, 1, 50};     //Waardes eigen stats in cijfers
uint8_t stats_opponent[4] = {0};      //Waardes stats tegenstander in cijfers
uint8_t id_opponent[3] = {0};         //ID's tegenstanders die ook aangesloten zijn aan GameServer
int current_position = 0;             //Houdt bij welke stat wordt weergegeven (Stats invoeren)
int points = 16;                      //Te verdelen punten over de stats
uint8_t hp;                           //Houdt levenspunten bij

int send_code = 0;            //Verzend code correspondeert met Command Table
int received_message = 0;     //Ontvangst code correspondeert met Command Table

String lcd_output[2];         //Text string voor weergave op LCD scherm

bool stats_send = false;      //Wordt gebruikt om stats maar één keer te verzenden
bool shown = false;           //Wordt gebruikt om LCD een keer per mode te refreshen
bool stats_opponent_shown = false; //Wordt gebruikt om stats tegenstander één keer weer te geven

LiquidCrystal lcd(12, 13, 8, 9, 10, 11);  //Setup LCD met juiste pinnen

void setup()
{
  Wire.begin(id);                // join i2c bus with clientID
  Wire.onRequest(requestEvent);  // Setup verzend bericht
  Wire.onReceive(receiveEvent);  // Setup onvang bericht

  Serial.begin(9600); // verwijderen uit release versie

  //Setup knoppen
  pinMode(button_action1, INPUT);
  pinMode(button_action2, INPUT);
  pinMode(button_action3, INPUT);
  pinMode(button_confirm, INPUT);
  Serial.println("Restarted");

  lcd.begin(16, 2); // Vertel de Arduino dat het LCD-display uit 16 kolommen en 2 rijen bestaat

  //Welkoms bericht
  lcd.clear();
  lcd_output[0] = "Verdeel punten";
  lcd_output[1] = "over de stats";
  update_lcd();
  delay(1500);

  lcd.blink(); //laat de cursor knipperen
  prepare_display_output();
}

void loop()
{
  switch (CurrentMode) {
    case Setup: {
        //Laat gebruiker stats invoeren en verbinden met GameServer

        if (digitalRead(button_action1) == HIGH && (millis() - changeTime) > 150) //add stats
        {
          add_stats();
          changeTime = millis();
        }

        if (digitalRead(button_action2) == HIGH && (millis() - changeTime) > 150) //subtract stats
        {
          subtract_stats();
          changeTime = millis();
        }

        if (digitalRead(button_action3) == HIGH && (millis() - changeTime) > 150) { //move screen to next item
          rotate_screen();
          prepare_display_output();
          changeTime = millis();
        }

        if (digitalRead(button_confirm) == HIGH && (millis() - changeTime) > 150) { //confirm & send stats
          hp = stats[3];
          lcd.noBlink();     //knipperende cursor uitzetten
          stats_bevestigd(); //verander scherm naar stats bevestigd
          switchToStandby();
          changeTime = millis();
        }
        break;
      }
    case SelectOpponent: {
        //Laat gebruiker tegenstander kiezen
        if (digitalRead(button_action1) == HIGH && (millis() - changeTime) > 150)
        {
          send_code = id_opponent[0]; // select opponent1
          switchToStandby();
          changeTime = millis();
        }
        if (digitalRead(button_action2) == HIGH && (millis() - changeTime) > 150)
        {
          send_code = id_opponent[1]; // select opponent2
          switchToStandby();
          changeTime = millis();
        }

        if ((digitalRead(button_action3) == HIGH && (millis() - changeTime) > 150) && id_opponent[2] != 0) {
          send_code = id_opponent[2]; // select opponent3
          switchToStandby();
          changeTime = millis();
        }
        break;
      }
    case Attack: {
        //Laat de gebruiker zijn attack kiezen
        if (!shown) { //voorkomt voortdurend updaten lcd
          if (!stats_opponent_shown) {
            show_stats_opponent();
            stats_opponent_shown = true;
          }
          show_hp();
          lcd_output[0] = "Choose attack:";
          lcd_output[1] = "ATK MAG STR";
          update_lcd();
          shown = true;
        }

        if (digitalRead(button_action1) == HIGH && (millis() - changeTime) > 150)
        {
          switchToReceiveHP();
          send_code = MOVE_1; //Attack
          changeTime = millis();
        }

        if (digitalRead(button_action2) == HIGH && (millis() - changeTime) > 150)
        {
          switchToReceiveHP();
          send_code = MOVE_3; //Magic
          changeTime = millis();
        }

        if (digitalRead(button_action3) == HIGH && (millis() - changeTime) > 150) {
          switchToReceiveHP();
          send_code = MOVE_2; //Strike
          changeTime = millis();
        }
        break;
      }
    case Defend: {
        //Laat de gebruiker zijn defence
        if (!shown) { //voorkomt voortdurend updaten lcd
          if (!stats_opponent_shown) {
            show_stats_opponent();
            stats_opponent_shown = true;
          }
          show_hp();
          lcd_output[0] = "Choose defense:";
          lcd_output[1] = "DEF MAG CTR";
          update_lcd();
          shown = true;
        }
        if (digitalRead(button_action1) == HIGH && (millis() - changeTime) > 150)
        {
          switchToReceiveHP();
          send_code = MOVE_1; //Defend
          changeTime = millis();
        }

        if (digitalRead(button_action2) == HIGH && (millis() - changeTime) > 150)
        {
          switchToReceiveHP();
          send_code = MOVE_2; //Magicblock
          changeTime = millis();
        }

        if (digitalRead(button_action3) == HIGH && (millis() - changeTime) > 150) {
          switchToReceiveHP();
          send_code = MOVE_3; //Counter
          changeTime = millis();
        }
        break;
      }
    case ReceiveHP: {
        if (!shown) {
          lcd_output[0] = "Waiting on other";
          lcd_output[1] = "player";
          update_lcd();
          shown  = true;
        }
        if (received_message == 0) {
          shown = false;
          CurrentMode = Death;
        }
        break;
      }
    case NextBattle: {
        //Laat winnaars bericht zien
        lcd_output[0] = "You won! :)";
        lcd_output[1] = "";
        update_lcd();
        delay(1500);


        if (NextMode != NextBattle) {
          lcd_output[0] = "Get ready for";
          lcd_output[1] = "the next battle";
          update_lcd();
          delay(1500);

          //reset do nodige variabelen
          hp = stats[3];  //reset hp op orginele waarde
          for (int i = 0; i < 3; i++) { //cleart array id_opponents
            id_opponent[i] = 0;
          }
          stats_opponent_shown = false;
          shown = false;

          CurrentMode = NextMode;
          NextMode = NextBattle;
        }
        break;
      }
    case Death: {
        if (!shown) {
          show_hp();
          lcd_output[0] = "You lost! :(";
          lcd_output[1] = "";
          update_lcd();
          shown  = true;
        }

        break;
      }
  }

}

void stats_bevestigd() {
  lcd_output[0] = "Standby mode";
  lcd_output[1] = "";
  update_lcd();
  delay(1500);
}

void add_stats() {
  if (points > 0) {
    if (current_position == 3) {
      stats[current_position] += 5;
    }
    else {
      stats[current_position]++;
    }
    points--;
    prepare_display_output();
  }
}

void subtract_stats() {
  if (current_position == 3) {
    if (stats[3] > 50) {
      stats[current_position] -= 5;
      points++;
    }
  }
  else {
    if (stats[current_position] > 1) {
      stats[current_position]--;
      points++;
    }
  }
  prepare_display_output();
}

void rotate_screen() { //Verschuift scherm naar volgend stat
  if (current_position == 3) {
    current_position = 0;
  }
  else {
    current_position++;
  }
}

void prepare_display_output() { //Zet stats om in strings, print het op LCD
  lcd_output[0] = lcd_positions[current_position] + convert_int_to_string(stats[current_position], current_position) + "  "; // linkerhelft bovenste regel op lcd
  if (current_position == 3) {
    if (convert_int_to_string(stats[3], 3).length() == 2 ) {
      lcd_output[0] += lcd_positions[0] + " " + convert_int_to_string(stats[0], 0);
    }
    else {
      lcd_output[0] += lcd_positions[0] + convert_int_to_string(stats[0], 0);
    }
  }
  else {
    lcd_output[0] += lcd_positions[current_position + 1] + convert_int_to_string(stats[current_position + 1] , current_position + 1);
  }
  lcd_output[1] = "Te verdelen:  " + convert_points_to_string(points); // onderste regel lcd
  update_lcd();
}

String convert_int_to_string(int getal, int positie) { //Verbetert tekstuitlijning voor stats
  String getal_string = (String)getal;
  if (getal_string.length() == 1) {
    getal_string = " " + getal_string; // bij single digits wordt er een spatie aan het begin toegevoegd
  }
  else if (getal_string.length() == 2 && positie == 3) {
    getal_string = " " + getal_string; // uitzondering voor HP
  }
  return getal_string;
}

String convert_points_to_string(int getal) { //Uitlijning te verdelen punten
  String getal_string = (String)getal;
  if (getal_string.length() == 1) {
    getal_string = " " + getal_string; // bij single digits wordt er een spatie aan het begin toegevoegd
  }
  return getal_string;
}

void update_lcd() { //Zet elk element van array lcd_output op een regel van het lcd scherm
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(lcd_output[0]);
  lcd.setCursor(0, 1);
  lcd.print(lcd_output[1]);
  lcd.setCursor(6, 0);
}

void requestEvent() { //Verzend berichten
  if (CurrentMode == Standby && !stats_send) {    //stats verzenden
    Wire.write(stats, 4);
    Serial.println("Bericht verzonden.");
    stats_send = true;
  }
  else if (send_code != NO_MESSAGE) {             //actie verzenden
    Wire.write(send_code);
    Serial.println("Code send: " + (String)send_code);
    send_code = 0;

  }
}

void receiveEvent(int howMany) { //Ontvang berichten
  uint8_t receive_array[5] = {0};
  int i = 0;

  //ontvang data
  while (1 <= Wire.available()) { // loop through all
    int c = Wire.read(); // receive byte as a int
    receive_array[i] = c;
    received_message = c;
    i++;
  }

  Serial.println("Receive_array: " + (String)receive_array[0] + "_" + (String)receive_array[1] +  "_" + (String)receive_array[2] + "_" +  (String)receive_array[3] +  "_" + (String)receive_array[4]);
  switch (CurrentMode) {
    case Standby: {
        if (receive_array[3] > 0) { //ontvang stats
          for (int j = 0; j < 5; j++) {
            if (j > 0) {
              stats_opponent[j - 1] = receive_array[j]; //skip id nummer, sla enkel de stats op
            }
          }
        }
        else if (receive_array[0] == CHOOSE_OPP) {
          switchToSelectOpponent();
        }
        else if (receive_array[0] == ATTACKER) {
          switchToAttack();
        }
        else if (receive_array[0] == DEFENDER) {
          switchToDefend();
        }
        else if (receive_array[0] == WINNER) {
          switchToNextBattle();
        }
        break;
      }
    case SelectOpponent: {
        if (receive_array[1] > 0) { //ontvang ids tegenstanders
          for (int j = 0; j < 3; j++) {
            id_opponent[j] = receive_array[j];
          }
        }
        break;
      }
    case ReceiveHP: {
        hp = receive_array[0];
        if (hp == 0) {
          shown = false;
          CurrentMode = Death;
        }
        else {
          switchToStandby();
        }
        break;
      }
    case NextBattle: {
        if (receive_array[3] > 0) { //ontvang stats
          for (int j = 0; j < 5; j++) {
            if (j > 0) {
              stats_opponent[j - 1] = receive_array[j]; //skip id nummer, sla enkel de stats op
            }
          }
        }
        else if (receive_array[1] > 0) { //ontvang ids
          for (int j = 0; j < 3; j++) {
            id_opponent[j] = receive_array[j];
          }
        }
        else if (receive_array[0] == CHOOSE_OPP) {
          NextMode = SelectOpponent;
        }
        else if (receive_array[0] == ATTACKER) {
          NextMode = Attack;
        }
        else if (receive_array[0] == DEFENDER) {
          NextMode = Defend;
        }
        break;
      }
  }
}

void show_stats_opponent() {
  if (stats_opponent[1] != 0) { // lezen voor stats tegenstander
    lcd_output[0] = "Je tegenstander";
    lcd_output[1] = "koos voor: ";
    update_lcd();
    delay(1500);

    lcd_output[0] = lcd_positions[0] + convert_int_to_string(stats_opponent[0], 0) + "  " + lcd_positions[1] + convert_int_to_string(stats_opponent[1], 1);
    if (convert_int_to_string(stats_opponent[3], 3).length() == 2) {
      lcd_output[1] = lcd_positions[3] + " " + convert_int_to_string(stats_opponent[3], 3) + "  " + lcd_positions[2] + convert_int_to_string(stats_opponent[2], 2);
    }
    else {
      lcd_output[1] = lcd_positions[3] + convert_int_to_string(stats_opponent[3], 3) + "  " + lcd_positions[2] + convert_int_to_string(stats_opponent[2], 2);
    }
    update_lcd();
    delay(5000);
  }
}

void show_hp() {
  lcd_output[0] = "Your hp: " + (String)hp;
  lcd_output[1] = "";
  update_lcd();
  delay(1500);
}

void switchToStandby() {
  shown = false;
  CurrentMode = Standby;
}

void switchToSelectOpponent() {
  lcd_output[0] = "Selecteer";
  lcd_output[1] = "tegenstander";
  update_lcd();
  CurrentMode = SelectOpponent;
}

void switchToAttack() {
  shown = false;
  CurrentMode = Attack;
}

void switchToDefend() {
  shown = false;
  CurrentMode = Defend;
}

void switchToReceiveHP() {
  shown = false;
  CurrentMode = ReceiveHP;
}

void switchToNextBattle() {
  shown = false;
  stats_opponent_shown = false;
  CurrentMode = NextBattle;
}

void resetter() { //Reset waardes om een nieuw toernooi te beginnen
  hp = 0;
  for (int i = 0; i < 4; i++) {
    stats_opponent[i] = 0;
  }
  for (int i = 0; i < 3; i++) {
    id_opponent[i] = 0;
  }
  stats_opponent_shown = false;
  shown = false;
  CurrentMode = Setup;
}

