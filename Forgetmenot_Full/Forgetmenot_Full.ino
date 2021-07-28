enum gameStates {SETUP, CENTER, SENDING, WAITING, PLAYING_PUZZLE, PLAYING_PIECE, ERR};
byte gameState = SETUP;
bool firstPuzzle = false;

enum answerStates {INERT, CORRECT, WRONG, RESOLVE};
byte answerState = INERT;

byte centerFace = 0;

//PACKET ARRANGEMENT: puzzleType, puzzlePalette, puzzleDifficulty, isAnswer, showTime, darkTime
byte showTime = 500;
byte darkTime = 200;
byte petalPacketStandard[6] = {0, 0, 0, 0, showTime, darkTime};
byte petalPacketPrime[6] = {0, 0, 0, 1, showTime, darkTime};

byte currentPuzzleLevel = 0;
Timer puzzleTimer;
Timer answerTimer;

//byte petalHues[4] = {131, 159, 180, 223};//light blue, dark blue, violet, pink

#define PINK2 makeColorRGB(255,50,100)
#define PINK4 makeColorRGB(255,100,255)
#define PINK6 makeColorRGB(255,200,255)
#define PINK3 makeColorRGB(255,0,100)
#define PINK1 makeColorRGB(255,50,0)
#define PINK5 makeColorRGB(150,50,255)

#define BLUE1 makeColorRGB(0,150,255)
#define BLUE2 makeColorRGB(50,100,255)
#define BLUE3 makeColorRGB(50,0,255)
#define BLUE4 makeColorRGB(100,10,255)
#define BLUE5 makeColorRGB(150,100,255)
#define BLUE6 makeColorRGB(200,200,255)

Color pinkColors[6] = {PINK1, PINK2, PINK3, PINK4, PINK5, PINK6};
Color blueColors[6] = {BLUE1, BLUE2, BLUE3, BLUE4, BLUE5, BLUE6};
Color primaryColors[4] = {RED, YELLOW, GREEN, BLUE};

bool canBloom = false;
Timer bloomTimer;
#define BLOOM_TIME 1000
#define GREEN_HUE 77
#define YELLOW_HUE 42


//Puzzle levels
// byte puzzleInfo[6] = {puzzleType, puzzlePalette, puzzleDifficulty, isAnswer, showTime, darkTime};

// colorPetals:  color changes on one of the petals
// locationPetals:  one side on each petal is lit, and changes position
// animationPetlas: a basic animation clockwise or counterclockwise on each petal... one changes
// globalPetals: a
enum puzzleType {colorPetals, locationPetals, animationPetals, globalPetals, 
                  flashPetals, changeingPetals, animationPetals2, numPetals};
enum puzzlePallette  {primary, pink, blue};

// beginner: pick from two colours
// easy: pick from three colours
// medium: pick from four colours
// hard: pick from five colours
// extrahard: pick from 6 colours --- probably not
enum puzzleDifficulty {beginner, easy, medium, hard, extrahard};




void setup() {
  // put your setup code here, to run once:
  randomize();
}

void loop() {
  switch (gameState) {
    case SETUP:
      setupLoop();
      setupDisplay();
      break;
    case CENTER:
    case SENDING:
    case PLAYING_PUZZLE:
      centerLoop();
      centerDisplay();
      break;
    case WAITING:
    case PLAYING_PIECE:
      pieceLoop();
      pieceDisplay();
      break;
    case ERR:
      break;
  }

  answerLoop();

  //do communication
  byte sendData = (gameState << 3) | (answerState);
  setValueSentOnAllFaces(sendData);

  //dump button presses
  buttonSingleClicked();
  buttonDoubleClicked();
  buttonMultiClicked();
}

void setupLoop() {
  bool emptyNeighbor = false;
  FOREACH_FACE(f) {
    if (isValueReceivedOnFaceExpired(f)) {//no neighbor
      emptyNeighbor = true;
    } else {
      if (getGameState(getLastValueReceivedOnFace(f)) == SENDING || getGameState(getLastValueReceivedOnFace(f)) == CENTER) {//this neighbor is telling me to play the game
        gameState = WAITING;
        centerFace = f;
      }
    }
  }

  if (emptyNeighbor == true) {
    canBloom = false;
  } else {
    if (canBloom == false) {
      bloomTimer.set(BLOOM_TIME);
    }
    canBloom = true;
  }

  if (canBloom) {
    if (buttonSingleClicked()) {
      gameState = CENTER;
      firstPuzzle = true;
    }
  }
}

void centerLoop() {
  if (gameState == CENTER) {
    //here we just wait for clicks to launch a new puzzle
    if (buttonSingleClicked() || firstPuzzle) {
      gameState = SENDING;
      generatePuzzle();
      firstPuzzle = false;
    }
  } else if (gameState == SENDING) {
    //here we just wait for all neighbors to go into PLAYING_PIECE

    byte piecesPlaying = 0;
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {//a neighbor! this actually needs to always be true, or else we're in trouble
        byte neighborData = getLastValueReceivedOnFace(f);
        if (getGameState(neighborData) == PLAYING_PIECE) {
          piecesPlaying++;
        }
      }
    }

    if (piecesPlaying == 6) {//all of the pieces have gone into playing, so can we
      gameState = PLAYING_PUZZLE;
    }

  } else if (gameState == PLAYING_PUZZLE) {
    //so in here, we just kinda hang out and wait to do... something?
    //I guess here we just listen for RIGHT/WRONG signals?
    //and I guess eventually ERROR HANDLING

    if (buttonSingleClicked()) {//Use a life and replay puzzle TODO
      
    }

    if (buttonDoubleClicked()) {//here we reveal the correct answer and move forward
      answerState = CORRECT;
      answerTimer.set(2000);   //set answer timer for display
      gameState = CENTER;
    }


  }
}

byte puzzleInfo[6] = {0, 0, 0, 0, 0, 0};
byte stageOneData = 0;
byte stageTwoData = 0;

void generatePuzzle() {
  byte primePiece = random(5);//which face will have the correct answer?

  //TODO: difficulty algorithm
  //needs to choose a puzzle type, a color scheme, set the timers, and

  FOREACH_FACE(f) {
    if (f == primePiece) {
      sendDatagramOnFace( &petalPacketPrime, sizeof(petalPacketPrime), f);
    } else {
      sendDatagramOnFace( &petalPacketStandard, sizeof(petalPacketStandard), f);
    }
  }
}

void pieceLoop() {
  if (gameState == WAITING) {//check for datagrams, then go into playing
    //listen for a packet on master face
    bool datagramReceived = false;

    if (isDatagramReadyOnFace(centerFace)) {//is there a packet?
      if (getDatagramLengthOnFace(centerFace) == 6) {//is it the right length?
        byte *data = (byte *) getDatagramOnFace(centerFace);//grab the data
        markDatagramReadOnFace(centerFace);
        FOREACH_FACE(f) {
          puzzleInfo[f] = data[f];
        }

        datagramReceived = true;
      }
    }


    if (datagramReceived) {
      gameState = PLAYING_PIECE;
      //quickly do some figuring out based on puzzle figuring
      stageOneData = determineStages(puzzleInfo[0], puzzleInfo[2], puzzleInfo[3], 1);
      stageTwoData = determineStages(puzzleInfo[0], puzzleInfo[2], puzzleInfo[3], 2);

      //BEGIN SHOWING THE PUZZLE!
      puzzleTimer.set((puzzleInfo[4] + puzzleInfo[5]) * 10);//the timing within the datagram is reduced
    }
  } else if (gameState == PLAYING_PIECE) {//I guess just listen for clicks and signals?
    if (buttonSingleClicked()) {
      //is this right or wrong?
      //TODO: actually have an answer to this. For now... we'll just do a 50/50 split
      //bool isCorrect = random(1);
      bool isCorrect = (puzzleInfo[3]!=0);

      if (isCorrect) {
        answerState = CORRECT;
      } else {
        answerState = WRONG;
      }
      answerTimer.set(2000);   //set answer timer for display
      gameState = WAITING;
    }
  }

}

byte determineStages(byte puzzType, byte puzzDiff, byte amAnswer, byte stage) {
  if (stage == 1) {
    //TODO: more complicated build here
    return (random(3)); //return (random(5));
  } else {//only change answer if amAnswer
    if (amAnswer) {
      //TODO: need to consider difficulty level here
      //return ((stageOneData + random(2) + 1) % 4); // moded for 4 color puzzle
      do {
        stageTwoData = random(3);
      } while( stageTwoData == stageOneData);
      return (stageTwoData);
    } else {
      return (stageOneData);
    }
  }
}

void answerLoop() {
  //so we gotta just listen around for all these signals
  if (answerState == INERT) {//listen for CORRECT or WRONG
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {
        byte neighborAnswer = getAnswerState(getLastValueReceivedOnFace(f));
        if (neighborAnswer == CORRECT || neighborAnswer == WRONG) {
          answerState = neighborAnswer;
          answerTimer.set(2000);

          if (gameState == PLAYING_PIECE) {
            gameState = WAITING;
          } else if (gameState == PLAYING_PUZZLE) {
            gameState = CENTER;
          }
        }
      }
    }
  } else if (answerState == CORRECT || answerState == WRONG) {//just wait to go to RESOLVE
    if (gameState == PLAYING_PIECE) {
      gameState = WAITING;
    } else if (gameState == PLAYING_PUZZLE) {
      gameState = CENTER;
    }

    bool canResolve = true;
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {
        byte neighborAnswer = getAnswerState(getLastValueReceivedOnFace(f));
        if (neighborAnswer == INERT) {
          canResolve = false;
        }
      }
    }

    if (canResolve) {
      answerState = RESOLVE;
    }
  } else if (answerState == RESOLVE) {//wait to go to INERT
    if (gameState == PLAYING_PIECE) {
      gameState = WAITING;
    } else if (gameState == PLAYING_PUZZLE) {
      gameState = CENTER;
    }

    bool canInert = true;
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {
        byte neighborAnswer = getAnswerState(getLastValueReceivedOnFace(f));
        if (neighborAnswer != INERT && neighborAnswer != RESOLVE) {
          canInert = false;
        }
      }
    }

    if (canInert) {
      answerState = INERT;
    }
  }
}

////DISPLAY FUNCTIONS

void setupDisplay() {
  if (canBloom) {

    byte bloomProgress = 255 - map(bloomTimer.getRemaining(), 0, BLOOM_TIME, 0, 255);

    byte bloomHue = map(bloomProgress, 0, 255, GREEN_HUE, YELLOW_HUE);
    byte bloomBri = map(bloomProgress, 0, 255, 100, 255);

    setColor(makeColorHSB(bloomHue, 255, bloomBri));
  } else {
    setColor(makeColorHSB(GREEN_HUE, 255, 100));
  }
}

void centerDisplay() {
  //so we need some temp graphics
  switch (gameState) {
    case CENTER:
      if (!answerTimer.isExpired()) {
        if (answerState == CORRECT) {
          setColor(GREEN);
        } else if (answerState == WRONG) {
          setColor(RED);
        }      
      } else {
        setColor(YELLOW);
        setColorOnFace(WHITE, random(5));
      }
      break;
    case SENDING:
      setColor(dim(YELLOW, 100));
      break;
    case PLAYING_PUZZLE:
      setColor(YELLOW);
      break;
  }
  //setColor(makeColorHSB(YELLOW_HUE, 255, 255));
  //setColorOnFace(makeColorHSB(YELLOW_HUE, 0, 255), random(5));
}

void pieceDisplay() {

  //TODO: break this into puzzle type specific displays
  if (gameState == WAITING) {//just waiting
    
    //setColor(OFF);
    //setColorOnFace(GREEN, centerFace);
    if (!answerTimer.isExpired()){
          if (answerState == CORRECT) {
             setColor(GREEN);
          } else if (answerState == WRONG) {
             setColor(RED);
          }
    } else {
      setColor(OFF);
      setColorOnFace(GREEN, centerFace);
    }
    
    
  } else {//show the puzzle
    if (puzzleTimer.isExpired()) {//show the last stage of the puzzle (forever)
      //TODO: take into account color palette, defaulting to pink for now
      setColor(primaryColors[stageOneData]); //setColor(pinkColors[stageOneData]);
    } else if (puzzleTimer.getRemaining() <= (puzzleInfo[5] * 10)) {//show darkness with a little flower bit
      setColor(OFF);
      setColorOnFace(dim(GREEN, 100), centerFace);
    } else {//show the first stage of the puzzle
    }
  }

  //temp answer display
  //  byte oppFace = (centerFace + 3) % 6;
  //  switch (answerState) {
  //    case INERT:
  //      setColorOnFace(WHITE, oppFace);
  //      break;
  //    case CORRECT:
  //      setColorOnFace(GREEN, oppFace);
  //      break;
  //    case WRONG:
  //      setColorOnFace(RED, oppFace);
  //      break;
  //    case RESOLVE:
  //      setColorOnFace(BLUE, oppFace);
  //      break;
  //  }
}

////CONVENIENCE FUNCTIONS

byte getGameState(byte data) {
  return (data >> 3);//returns the 1st, 2nd, and 3rd bit
}

byte getAnswerState(byte data) {
  return (data & 7);//returns the 5th and 6th bit
}
