/*

Update 666.01

Gamby Bloc Drop Xtr3m update for foos

Updates and fixes for the very familiar falling block game.

By DWhacks, demonicpancake & Chiermaster




BLOC DROP: A strangely familiar falling block game
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
by David Randall Stokes (gamby@logicalzero.com) 9/05/2012
 
THIS IS STILL A WORK-IN-PROGRESS. The game is playable, but 
the scoring doesn't work correctly.
 
*/
 
 
#include <Gamby.h>
#include <avr/pgmspace.h>
 
//////////////////////////////////////////////////////////////////////////////
// The falling block assemblies. There are 7 different shapes. Each 
// orientation of a piece (0, 90, 180 and 270 degrees) is actually another 
// shape, which is why they are stored in a 2D array (pieces[shape][rotation]). 
// Each piece (and each orientation of each piece) is stored as a 4x4 grid, 
// even though most are smaller. Since these are a 4x4 grid, they are stored as
// 16b unsigned integers, just like blocks and GambyGraphicsMode fill patterns.
unsigned int pieces[7][4] = {
  { 0x0720, 0x2620, 0x2700, 0x4640 }, // "T"  now its good DW **DRU**0
  { 0x2222, 0x0f00, 0x2222, 0x0f00 }, // Bar
  { 0x2310, 0x3600, 0x2310, 0x3600 }, // 'S' zig-zag
  { 0x1320, 0x6300, 0x1320, 0x6300 }, // 'Z' zig-zag
  { 0x4700, 0x3220, 0x7100, 0x2260 }, // 'J'
  { 0x7400, 0x6220, 0x1700, 0x2230 }, // 'L'
  { 0x3300, 0x3300, 0x3300, 0x3300 }  // 2x2 square
  //{ 0x8f80, 0xcc44, 0x1710, 0x44cc }  // SURPRISE **DRU**10
};
 
//////////////////////////////////////////////////////////////////////////////
// The GambyBlockMode block palette.
// Each piece has its own block type: falling piece `n` uses block `n+1`. 
// There is a duplicate of each block used for 'dropped' pieces (`n+8`); this
// makes it easy to ignore the currently falling piece's blocks when testing 
// for collisions.
PROGMEM prog_uint16_t palette[] = {
  0x0000, //  0: Empty
  0xf99f, //  1: Block 0 ("T")
  0xadaf, //  2: Block 1 ("-")
  0xfdbf, //  3: Block 2 ('S')
  0xfbdf, //  4: Block 3 ('Z')
  0xfafd, //  5: Block 4 ('J')
  0xdadf, //  6: Block 5 ('L')
  0xf9df, //  7: Block 6 (2x2)
  0xf99f, //  8: Block 0 dropped
  0xadaf, //  9: Block 1 dropped
  0xfdbf, // 10: Block 2 dropped
  0xfbdf, // 11: Block 3 dropped
  0xfafd, // 12: Block 4 dropped
  0xdadf, // 13: Block 5 dropped
  0xf9df, // 14: Block 6 dropped
  0xffff, // 15: "Well" wall
};
 
//////////////////////////////////////////////////////////////////////////////
 
// Constants to be used when rotating a piece. Since bytes are only positive
// and there are only 4 orientations, counter-clockwise is 3; the number
// will 'wrap around' to get to the previous number.
const byte ROTATE_CW = 1;
const byte ROTATE_CCW = 3;
 
// Some game-related constants
const byte WELL_WIDTH = 12;
const byte WELL_BOTTOM = 15;
const int DEFAULT_SPEED = 1000;
const byte INPUT_DELAY = 50;
const byte START_X = 4, START_Y = 0;
const byte NEXT_X = NUM_BLOCK_COLUMNS - 4, NEXT_Y = 0;
 
boolean playing;    // 'true' when the game is playing, 'false' after game over
 
byte currentPiece;  // Index of the falling piece in the pieces 'outer' array
byte dir;           // orientation of the falling piece ('inner' piece array)
byte x, y;          // The upper left corner of the piece's 4x4 grid
unsigned long timeToDrop;    // Time remaining (ms) before the piece drops a level
unsigned long dropSpeed;     // The speed of the drop, changes with skill level
int score;          // The player's score
int highScore;      // Self-explanatory
int level;
int linesCleared;
byte nextPiece;     // Index of the next piece to fall (see currentPiece)
int pieceCount;     // The number of pieces that have fallen
 
unsigned long lastInputTime; // The time at which gamby.readInputs() was last called
byte lastInputs;    // The state of the inputs the last time they were checked.
 
// Bring in the font from the other tab (font.ino)
extern prog_int32_t font[];
 
GambyBlockMode gamby;
 
 
//////////////////////////////////////////////////////////////////////////////
 
void setup() {
  gamby.palette = palette;
  gamby.font = font;
  
  showSplashScreen();
 
  // Reset the random number generator. With the splash screen showing 
  // first, differences in starting time should make this pretty well 
  // randomized.
  randomSeed(millis());
 
  startGame();
}
 
 
void loop() {
  if (playing) {
    // Read the inputs if enough time has passed
    if (millis() > lastInputTime) {
      gamby.readInputs();
      
      // Moving left/right and rotating don't auto-repeat, but dropping does
      if (gamby.inputs & DPAD_DOWN || gamby.inputs != lastInputs) {
        lastInputs = gamby.inputs;
      
        if (gamby.inputs & DPAD_LEFT)
          movePiece(x-1, y, dir);
        else if (gamby.inputs & DPAD_RIGHT)
          movePiece(x+1, y, dir);
        else if (gamby.inputs & DPAD_DOWN)
          timeToDrop = 0;
        //else if (gamby.inputs & DPAD_UP)
         // timeToDrop = millis(); // HARD DROP. (0 might not be right tho, not sure if coordinate will just go at bottom or stop when reaching ) **DRU**1
        else if (gamby.inputs & BUTTON_2)
          rotate(ROTATE_CW);              //(btn Right) TURN CLOCKWIZE **DRU**
        else if (gamby.inputs & BUTTON_3)
          rotate(ROTATE_CCW);             // (btn left) TURN COUNTER-CLOCKWIZE
        
        lastInputTime = millis() + INPUT_DELAY;
      }
    }
 
    // If it's time, drop the piece down a row. 
    if (millis() > timeToDrop)
      dropPiece();
  }
  else {
    // After 'game over'; wait for a button press to restart.
    delay(100);
    gamby.readInputs();
    if (gamby.inputs)
      startGame();
  }
}
 
//////////////////////////////////////////////////////////////////////////////
 
// Start a new game.
//
//
void startGame() {
  byte i;
  
  // 1. Draw the background
  gamby.clearScreen();
  
  // The vertical sides of the 'well'
  for (i=0; i<WELL_BOTTOM; i++) {
    gamby.setBlock(0,i,15);
    gamby.setBlock(WELL_WIDTH-1, i, 15);
  }
  // The well's bottom
  for (i=0; i<WELL_WIDTH; i++) {
    gamby.setBlock(i, WELL_BOTTOM, 15);
  }
  // setBlock() doesn't immediately draw; update() shows all the 
  // changes at the same time.
  gamby.update(0,0,WELL_WIDTH-1,WELL_BOTTOM);
  
  // Draw the rest of the screen (the 'Next:' text, etc.)
  gamby.setPos(52,6);
  gamby.print("Score:");
  gamby.setPos(52,7); 
  gamby.print(score); //prints score to 0
  
  // Draw Level on the screen **DRU**6 DW moved to the middle
  gamby.setPos(52,4);
  gamby.print("Level:");
  gamby.setPos(52,5);
  gamby.print(level);
  gamby.setPos(52,2);
  gamby.print("Cleared:");
  gamby.setPos(52,3);
  gamby.print(linesCleared);
  
  // 2. Reset some of the player variables
  score = 0;
  level = 0;
  linesCleared = 0;  
  x = START_X;
  y = START_Y;
  dropSpeed = DEFAULT_SPEED; // todo: make this change by 'level'.
  playing = true;
  
  // 3. Add the first piece. 
  nextPiece = random(7);
  addNewPiece();
}
 
 
// End the game. Called when a new block doesn't have room
// to be added.
//
void gameOver() {
  playing = false;
  showGameOver();
 }
 
 
// Start a new piece at the top of the 'well,' using the shape in the
// 'next piece' preview. A new 'next piece' is picked and drawn. The
// 'drop clock' is also reset.
void addNewPiece() {
  // Erase 'next piece' preview
  drawPiece(nextPiece, 0, NEXT_X, NEXT_Y, 0);
  currentPiece = nextPiece;
  
  // Pick a new 'next piece' that's not the same as the current one.
  // In theory, this loop could run forever, but that's really unlikely.
  //while (nextPiece == currentPiece) We do want the same as previous if random...
    nextPiece = random(7); //7 not 6... 6 excludes de 4x4 block...
  
  // Draw the new preview
  gamby.setPos(52,0);
  gamby.print("Next:");
  drawPiece(nextPiece, 0, NEXT_X, NEXT_Y, nextPiece+1);
  
  // Reset the rest of the piece-related variables
  x = START_X;
  y = START_Y;
  dir = 0;
  timeToDrop = millis() + dropSpeed;
  
  // If adding a piece fails (clips something), then the 'well' is
  // full -- the game is over.
  if (!movePiece(x, y, dir))
    gameOver();
 
  // Everything's good, so increment the total piece count.
  pieceCount++;
}
 
 
// Move the piece to a new position (or rotation), if that new
// position/direction doesn't hit anything. If it does clip something
// (dropped blocks or the sides of the well), the piece isn't moved.
// This will usually be called with only one of the 3 arguments different
// from the current position/direction.
boolean movePiece(byte newX, byte newY, byte newDir) {
  // See if the piece in the new position/rotation hits something.
  // If it does, return false.
  if (checkHits(newX, newY, newDir))
    return false;
  
  // The piece can be moved safely. Erase the piece in its old position and
  // redraw it in its new position.
  drawPiece(currentPiece, dir, x, y, 0);
  dir = newDir;
  x = newX;
  y = newY;
  drawPiece(currentPiece, dir, x, y, currentPiece+1);
  
  // 
  return true;
}
 
 
// Draw a piece. The position, direction, shape and block type are all
// parameters; this is used to erase pieces as well as to show the
// 'next' piece, not just the currently falling one.
void drawPiece(byte piece, byte d, byte px, byte py, byte block) {
  byte i,j;
  for (i=0; i<4; i++) {
    for (j=0; j<4; j++) {
      if (getPieceBlock(piece, d, j, i)) {
        gamby.setBlock(px+j, py+i, block);
      }
    }
  }
  gamby.update(px, py, px+3, py+3);
}
 
 
// Given a piece at a certain orientation -- which are all 4x4 grids --
// determine if one of the squares in that grid is filled.
//
boolean getPieceBlock(byte piece, byte d, byte px, byte py) {
  // This is overly complex due to the fact that the pieces are stored like
  // GAMBY blocks/patterns: bottom to top, left to right.
   return (pieces[piece][d] & (1 << (15 - ((3-py) + (px << 2))))) != 0;
}
 
 
// Attempt to rotate the currently falling piece.
//
//
void rotate(byte d) {
  // This just wraps movePiece(), hiding the weirdness in the way
  // the rotation is done.
  movePiece(x, y, (dir + d) & B00000011);
}
 
 
// Check if the peice will hit something if moved or rotated.
//
//
boolean checkHits(byte newX, byte newY, byte newDir) {
  byte i,j;
  for (i=0; i<4; i++)
//    if ((newY + i) < 15)
      for (j=0; j<4; j++)
        if (getPieceBlock(currentPiece, newDir, j, i) && 
            gamby.getBlock(newX+j,newY+i) > 7)
              return true;
  return false;
}
 
 
//
//
//
void dropPiece() {
  if (!movePiece(x, y+1, dir)) {
    // Draw in the falling piece's blocks using the 
    // identical-looking 'static' version.
    drawPiece(currentPiece, dir, x, y, currentPiece+8);
    int c = checkCompleteRows(); //still trying to avoid converting data all the time **DRU**
	if (c > 0) { //Check if any lines are cleared
		getScore(c); //If so, Score!.
	}
    gamby.clearLine();
    addNewPiece();
  }
  timeToDrop = millis() + dropSpeed;
}
 
 
// Clear filled rows, shifting down everything above each cleared row.
// The number of rows cleared is returned.
//
byte checkCompleteRows() {
  int cleared;  // The number of cleared rows //Back to byte
  byte c;        // The number of blocks per row
  byte i, j;
  
  for (i=0; i<WELL_BOTTOM; i++) {
    byte c = 0;
    for (j=1; j<WELL_WIDTH-1; j++) {
      if (gamby.getBlock(j,i) > 7) {
        c++;
      }
    }
    // If the row is full, slide everything down.
    if (c == WELL_WIDTH-2) {
      byte m, n;
      for (m=1; m<WELL_WIDTH-1; m++) {
        for (n=i; n>0; n--) {
          gamby.setBlock(m, n, gamby.getBlock(m,n-1));
        }
        gamby.setBlock(m, 0, 0);
      }
      cleared++;
      linesCleared++;
    }
  }
  
  // Draw all the changes made with setBlock()
  gamby.update(1,0,WELL_WIDTH-1,WELL_BOTTOM-1);
  return cleared;
}

//
void getScore(int c) {
	 //number of lines has to be integer to work with score formula
					//Not sure if var c is out scope here. might have to call it again.
	switch(c) {	//Many points per line, Like original nintendo Tetris on NES/SNES/GB
		case 1:
		  score += 40 * (level+1); // **DRU**4
                  break;
		case 2:
		  score += 100 * (level+1);
                  break;
		case 3:
		  score += 300 * (level+1);
                  break;
		case 4: //TETRIS MUTHA FUCKA! **DRU**5
		  score += 1200 * (level+1);
		  gamby.setPos(52,3); //This could be erased..LOL
		  for (int t=0; t<1; t++) { //this too...
			gamby.print("TETRIS");
			delay(250);
			gamby.print("");
			delay(10);
			}
                  break;
	}
    getLevel();
    gamby.setPos(52,7);
    gamby.print(score, DEC);
    gamby.setPos(52,3);
    gamby.print(linesCleared, DEC);
}

// Level up
void getLevel() {
    int currentLevel = level;
    level = linesCleared / 10;
    if (currentLevel < level){
      gamby.setPos(52,5); //LEVELS !!! dw **DRU**7
      gamby.print(level, DEC);
      dropSpeed = dropSpeed * .85;
      dropSpeed += level/5;
    }
}

