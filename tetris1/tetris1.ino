//Computer Architecture
//Final Project
//Devon Dickson
//
// Button debounce code is from Adafruit:
// http://www.adafruit.com/blog/2009/10/20/example-code-for-multi-button-checker-with-debouncing/

// Also required are the Adafruit_RGB-matrix-panel library and the Adafruit_GFX library
// https://github.com/adafruit/Adafruit-GFX-Library
// https://github.com/adafruit/RGB-matrix-Panel
/*****************************************************************************************/
#include <RGBmatrixPanel.h>
#include <Adafruit_GFX.h>
#define DEBOUNCE 10

// The input buttons are stored in a byte array.
byte buttons[] = {10,11,12,13};
// 10 = rotate
// 11 = move right
// 12 = drop
// 13 = move left
#define NUMBUTTONS sizeof(buttons)
volatile byte pressed[NUMBUTTONS], justpressed[NUMBUTTONS], justreleased[NUMBUTTONS];
//I'm using interrupts for the buttons, so a volatile type is neccesary. 

//Pin assignments for LED Matrix.
#define CLK 8
#define LAT A3
#define OE  9
#define A   A0
#define B   A1
#define C   A2

RGBmatrixPanel matrix(A, B, C, CLK, LAT, OE, false);


int score;
int totalLines; // number of lines cleared
int level;  // the current level, should be totalLines/10
int stepCounter; // increments every loop. Resets after the value of gravityTrigger is reached
int gravityTrigger; // the active piece will drop one step after this many steps of stepCounter;

//theGrid is a map of all the pieces which have landed in the well. Originally it was an array of bytes (B00000000), but the well is 16 LEDs wide.
//By storing it as a short hexadecimal number, I get the equivalent of a 16bit number. Its just a little harder to visualize. 
short theGrid[] = {   
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000,
  0x0000
};


int activeShape; // the index of the currently active shape
int currentRotation=0; // the index of the current piece's rotation

int yOffset=0; // how far from the top the active shape is
int xOffset=0; // how far from the left the active shape is

//Here we have an array of every piece in every possible rotation. 
int shapes[7][4][4][2]={
/*  square  */    {
  /* angle 0   */   {  {1,0}, {2,0}, {1,1}, {2,1} },
  /* angle 90  */   {  {1,0}, {2,0}, {1,1}, {2,1} },
  /* angle 180 */   {  {1,0}, {2,0}, {1,1}, {2,1} },
  /* angle 270 */   {  {1,0}, {2,0}, {1,1}, {2,1} }
  },
/*  bar  */        {
  /* angle 0   */   {  {0,1}, {1,1}, {2,1}, {3,1} },
  /* angle 90  */   {  {1,0}, {1,1}, {1,2}, {1,3} },
  /* angle 180 */   {  {0,1}, {1,1}, {2,1}, {3,1} },
  /* angle 270 */   {  {1,0}, {1,1}, {1,2}, {1,3} }
  },  
/*  Z  */       {
  /* angle 0   */   {  {0,0}, {1,0}, {1,1}, {2,1} },
  /* angle 90  */   {  {2,0}, {1,1}, {2,1}, {1,2} },
  /* angle 180 */   {  {0,0}, {1,0}, {1,1}, {2,1} },
  /* angle 270 */   {  {2,0}, {1,1}, {2,1}, {1,2} }
  },   
/*  S  */       {
  /* angle 0   */   {  {1,0}, {2,0}, {0,1}, {1,1} },
  /* angle 90  */   {  {0,0}, {0,1}, {1,1}, {1,2} },
  /* angle 180 */   {  {1,0}, {2,0}, {0,1}, {1,1} },
  /* angle 270 */   {  {0,0}, {0,1}, {1,1}, {1,2} }
  },
/*  L  */       {
  /* angle 0   */   {  {0,1}, {1,1}, {2,1}, {0,2} },
  /* angle 90  */   {  {1,0}, {1,1}, {1,2}, {2,2} },
  /* angle 180 */   {  {2,0}, {0,1}, {1,1}, {2,1} },
  /* angle 270 */   {  {0,0}, {1,0}, {1,1}, {1,2} }   
  },
/*  J  */       {
  /* angle 0   */   {  {0,1}, {1,1}, {2,1}, {2,2} },
  /* angle 90  */   {  {1,0}, {2,0}, {1,1}, {1,2} },
  /* angle 180 */   {  {0,0}, {0,1}, {1,1}, {2,1} },
  /* angle 270 */   {  {1,0}, {1,1}, {1,2}, {0,2} }                       
  },
/* T  */{
  /* angle 0   */   {  {0,1}, {1,1}, {2,1}, {1,2} },
  /* angle 90  */   {  {1,0}, {1,1}, {2,1}, {1,2} },
  /* angle 180 */   {  {1,0}, {0,1}, {1,1}, {2,1} },
  /* angle 270 */   {  {1,0}, {0,1}, {1,1}, {1,2} } 
  }         
};

/********************************************************************************/

void startGame(){
  gravityTrigger=20;                     // reset the trigger point at which the active piece will drop another step
  level=1;                               // reset the level
  score=0;                               // reset the score
  totalLines=0;                          // reset the line count
  for (int row=0; row<32; row++){        // reset the fixed-piece grid
    theGrid[row]=0x0000;
  }

  void launchNewShape();                 // Drop the first shape
}


/********************************************************************************/
// When you are spinning a piece, it might end up colliding with another piece or slipping off thge edge of the well. This method checks to make sure your next move is OK.
boolean checkNextMove(int nextRot , int nextXOffset, int nextYOffset){

  boolean isOK=true;  // assume the move is valid. we'll change isOK if it's not

  if (nextRot>3){
    nextRot=0;
  };

  for (int thisPixel=0; thisPixel < 4; thisPixel++){  // Check all 4 pixels of the shape in play    
    // Out of bounds check.
    if (shapes[activeShape][nextRot][thisPixel][0]+nextXOffset >(15)||shapes[activeShape][nextRot][thisPixel][0]+nextXOffset<0){
      isOK=false;
      break;
    }

    //Bottom of Well check.
    if (shapes[activeShape][nextRot][thisPixel][1]+nextYOffset >(31)){
      isOK=false;       
      break;
    }

    //Collision check, the beast!   
    int inverseCol=15-(shapes[activeShape][nextRot][thisPixel][0]+nextXOffset); // Setting up, don't change these, everything breaks :)
    int theRow=(shapes[activeShape][nextRot][thisPixel][1])+nextYOffset;
    //if(bitRead(theGrid[theRow],inverseCol)){
    if(theGrid[theRow] & (1<<inverseCol)) { //Bitwise operations! I'll explain how it works with the theGrid later.
      isOK=false;
      break; //no need to check further
    }
  }
  return isOK;  
}

/********************************************************************************/
//When movement is done, write the final placement of the active piece to the fixed grid
void storeFinalPlacement(){
  for (int thisPixel=0; thisPixel < 4; thisPixel++){                            //run through all 4 pixels of the shape  
    int pixelX=shapes[activeShape][currentRotation][thisPixel][0] + xOffset;    //calculate its final, offset X position
    int pixelY=shapes[activeShape][currentRotation][thisPixel][1] + yOffset;    //calculate its final, offset Y position
    theGrid[pixelY] |= (1<<(15-pixelX)); //I used to use a bitset, but that only works with the Byte type, so now I am using a short hexadecimal. 
    //theGrid[pixelY] refers to the number 0x0000. Imagine the same number written in Binary is 0000000000000000. The above code will set the the (15-PixelX) digit, witht he rightmost being 0, to a one.
    //SO if a square falls, it might set the bottom row as 0000000000000011. Converted to hexadecimal, this is 0x0003, which is what gets stored in theGrid[pixelY]
  }
}

/********************************************************************************/
/* Launch a new shape from the top of the screen */
void launchNewShape(){
  activeShape=random(7); // pick a "random" shape
  Serial.println(activeShape);
  yOffset=0;             // reset yOffset so it comes from the top 
  xOffset=8;             // reset xOffset so it comes from the middle
  currentRotation=0;     // pieces start from the default rotation
  gravityTrigger=max(1,21-level);   // set the gravity appropriately. this is so that a drop doesn't carry to the next piece


  //Checks if the well is full, and the game is over.
  if (!checkNextMove(currentRotation, xOffset, yOffset)){
    endGame();
    displayScores();
    matrix.fillScreen(matrix.Color333(0,0,0));
    delay(5000);  //wait 5 seconds before restarting
    startGame();
  }        
}

/********************************************************************************/
void endGame(){
  short blankGrid[32]= //create a blank grid to use for blinking
  {   
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000,
    0x0000
  };
  for (int blinkCount=0; blinkCount<3; blinkCount++){
    //blink the fixed pieces 3 times by alternating between the blank grid and the played grid 
    // this section for blank grid
    matrix.fillScreen(matrix.Color333(0,0,0));
    drawFixedPieces(blankGrid);
    delay(150);
    matrix.fillScreen(matrix.Color333(0,0,0));
    drawFixedPieces(theGrid);
    delay(150);
  }
}

/********************************************************************************/
/* Check for completed rows and clear them, then drop pieces above */
void dropFullRows(){
  short theGridBlink[32];
  int fullRows[4]; 
  int fullRowCount=0;
  for (int row=0; row<32; row++){
    if (theGrid[row]==0xffff){ //equivalent to 1111111111111111
      theGridBlink[row]=0x0000;
      fullRows[fullRowCount]=row;    // record the position of the filled row
      fullRowCount++; 
    }
    else{
      theGridBlink[row]=theGrid[row];
    }
  }


  if (fullRowCount>0){
    for (int blinkCount=0; blinkCount<3; blinkCount++){ 
      matrix.fillScreen(matrix.Color333(0,0,0));
      drawFixedPieces(theGridBlink);
      delay(150);
      matrix.fillScreen(matrix.Color333(0,0,0));
      drawFixedPieces(theGrid);
      delay(150);
    }
  //shift rows down after clearing
    for (int i=0; i<fullRowCount; i++){     
      for (int copyRow=fullRows[i]; copyRow>0; copyRow--){
        theGrid[copyRow]=theGrid[copyRow-1];
      }
      theGrid[0]=0x0000;
    }


    totalLines=totalLines+fullRowCount; 

    int thePoints; 
    switch(fullRowCount){
    case 1:  // Single = 100 x Level
      thePoints=100 * level;
      break;
    case 2:  // Double = 300 x Level
      thePoints=300 * level;
      break;
    case 3:  // Triple = 500 x Level
      thePoints=500 * level;
      break;
    case 4:  // Tetris = 800 x Level
      thePoints=800 * level; 
      break;
    }

    score=score+thePoints;

    if (totalLines%10==0){
      level++;
      gravityTrigger--; // This makes it faster with every level 
    }
  }
}

/*********************************************************************************/
void displayScores(){
  matrix.fillScreen(matrix.Color333(0,0,0));


  String levelString=String(level);
  String pointString=String(score);

  String theLevel=String("Level " + levelString); 
  String theScore=String("Points " + pointString);

  
    matrix.setTextSize(1);
    matrix.setCursor(1,0);
    matrix.print(theLevel);
    delay(2000);
    //Not working or maybe just unreadable for some reason :(
    //matrix.setCursor(1, 0);
    //matrix.print('the score');
    //delay(2000);

}


/*********************************************************************************/
void drawFixedPieces(short gridToDraw[32]){

  for (int row=0; row<32; row++){
    for (int col=0; col <16; col++){
      if(gridToDraw[row] & (1<<(15-col))) { //Logical shifts. So hot right now. Logical Shifts.
        matrix.drawPixel(col, row, matrix.Color333(7,0,0));
      }
    }
  } 
}

/********************************************************************************/
void drawActivePiece(){
  for (int thisPixel=0; thisPixel < 4; thisPixel++){
    int pixelX=shapes[activeShape][currentRotation][thisPixel][0]; 
    int pixelY=shapes[activeShape][currentRotation][thisPixel][1]+yOffset;
    //if(activeShape = 
    matrix.drawPixel(pixelX + xOffset, pixelY, matrix.Color333(7,0,0));
  } 
}


/********************************************************************************/
void check_switches()
{
  static byte previousstate[NUMBUTTONS];
  static byte currentstate[NUMBUTTONS];
  static long lasttime;
  byte index;

  if (millis() < lasttime) {
    lasttime = millis();
  }

  if ((lasttime + DEBOUNCE) > millis()) {
    return;
  }
  lasttime = millis();

  for (index = 0; index < NUMBUTTONS; index++) {

    currentstate[index] = digitalRead(buttons[index]);
    if (currentstate[index] == previousstate[index]) {
      if ((pressed[index] == LOW) && (currentstate[index] == LOW)) {
        // just pressed
        justpressed[index] = 1;
      }
      else if ((pressed[index] == HIGH) && (currentstate[index] == HIGH)) {
        // just released
        justreleased[index] = 1;
      }
      pressed[index] = !currentstate[index];
    }
    previousstate[index] = currentstate[index];
  }
}
/********************************************************************************/
/********************************************************************************/

void setup()   
{   

  stepCounter=0;
  randomSeed(analogRead(0)); //I love how this seems to do absolutely nothing. 

  matrix.begin();
  matrix.setRotation(3);

  byte i;
  for (i=0; i< NUMBUTTONS; i++) {
    pinMode(buttons[i], INPUT);
    digitalWrite(buttons[i], HIGH);
  }
  //Don't ask, don't tell
  // Run timer2 interrupt every 15 ms 
  TCCR2A = 0;
  TCCR2B = 1<<CS22 | 1<<CS21 | 1<<CS20;

  //Timer2 Overflow Interrupt Enable
  TIMSK2 |= 1<<TOIE2;
  startGame();
}

SIGNAL(TIMER2_OVF_vect) {
  check_switches();
}
/********************************************************************************/
void loop()                   
{  
  for (byte i = 0; i < NUMBUTTONS; i++) {

    if (justpressed[i]) {
      justpressed[i] = 0;     
      switch (i){
      case 0: // rotate
        if (checkNextMove(currentRotation+1, xOffset, yOffset)){
          currentRotation++;
          if (currentRotation>3){
            currentRotation=0;
          };
        }; 
        break;
      case 1: // move right
        if (checkNextMove(currentRotation, xOffset+1, yOffset)){
          xOffset++;
        };
        break;

      case 2: // Drop faster
        gravityTrigger=min(3, gravityTrigger/2); // increase the speed of the drop to the lowest of either 3 or 1/2 the current speed
        // it's probably not the method used in the real game, but it was a quick solution 
        break;

      case 3: // move left
        if (checkNextMove(currentRotation, xOffset-1, yOffset)){
          xOffset--;  
        }
        break;
      } 
    }

    if (justreleased[2]) {
      gravityTrigger=max(1,21-level);
      justreleased[2]=0; 
    }

    if(stepCounter>gravityTrigger){ 
      stepCounter=0; 

      if (checkNextMove(currentRotation, xOffset, yOffset+1)){ 
        yOffset++; 

      }
      else{
        storeFinalPlacement(); 
        dropFullRows(); 
        launchNewShape();
      }
    }
    stepCounter++; 


    matrix.fillScreen(matrix.Color333(0,0,0)); 
    drawFixedPieces(theGrid);
    drawActivePiece(); 

    delay(30); 


  }
}

