///////////////////////////////////////////////////////////////////////////////////////////////////
//
// STEWARTS CHESS PROGRAMME
//
// Author: Stewart Tunbridge, Pi Micros
// Email:  stewarttunbridge@gmail.com
// Copyright (c) 2024 Stewart Tunbridge, Pi Micros
//
///////////////////////////////////////////////////////////////////////////////////////////////////


const char Revision [] = "0.99";

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "../Widgets/Widgets.hpp"
#include "../Widgets/WidgetsGrid.hpp"
#include "../Widgets/FileSelect.hpp"

#include "../ConsoleApps/chess/Chess.c"

void DebugAdd (const char* Message)
  {
    static int Time0 = ClockMS ();
    static int Time1 = Time0;
    char *s = (char *) malloc (StrLen (Message) + 32);
    char *ps = s;
    int t;
    //
    t = ClockMS ();
    NumToStrDecimals (&ps, t - Time0, 3);
    StrCat (&ps, " [");
    NumToStrDecimals (&ps, t - Time1, 3);
    Time1 = t;
    StrCat (&ps, "] ");
    StrCat (&ps, Message);
    *ps = 0;
    //
    puts (s);
    //Log ("Log", s);
    free (s);
  }

void CoordToStr (char **s, _Coord Pos)
  {
    *(*s)++ = Pos.x + 'a';
    *(*s)++ = Pos.y + '1';
  }

char *BlackPieceToChar = " lwtnjo";   // pEmpty, pKing, pQueen, pRook, pBishop, pKnight, pPawn
char *WhitePieceToChar = " kqrbhp";

char PieceToChar (int Pce)
  {
    if (PieceWhite (Pce))
      return WhitePieceToChar [Piece (Pce)];
    return BlackPieceToChar [Piece (Pce)];
  }

int MoveCount;
int BoardColour;
bool Refresh;
bool Exit;
bool Load;
bool Save;

bool PlayThreadWhite;
bool PlayThreadStarted;
bool PlayThreadFinished;
int PlayThreadScore;
int PlayThreadTime;

void *PlayThread (void *PlayWhite)
  {
    PlayThreadWhite = (bool) PlayWhite;
    PlayThreadStarted = true;
    PlayThreadFinished = false;
    MovesConsidered = 0;
    PlayThreadTime = ClockMS ();
    PlayThreadScore = BestMove (PlayThreadWhite, 0);
    PlayThreadTime = ClockMS () - PlayThreadTime;
    PlayThreadFinished = true;
    PlayThreadStarted = false;
    return NULL;
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BOARD VISUAL COMPONENT
//
////////////////////////////////////////////////////////////////////////////////////////////////////

class _ChessBoard: public _Container
  {
    public:
      int Colours [2];
      bool MoveStart, MoveComplete;
      _Coord MoveFrom, MoveTo;   // Human move
      _Coord PCFrom = {-1, -1}, PCTo;   // PC Move
      //
      _ChessBoard (_Container *Parent, _Rect Rect);
      ~_ChessBoard ();
      void DrawCustom (void);
      bool ProcessEventCustom (_Event *Event, _Point Offset);
  };

_ChessBoard::_ChessBoard (_Container *Parent, _Rect Rect) : _Container (Parent, Rect)
  {
    Colours [0] = ColourAdjust (cBrown, 140);
    Colours [1] = ColourAdjust (cBrown, 180);
    MoveStart = MoveComplete = false;
  }

_ChessBoard::~_ChessBoard ()
  {
  }

const int cGreenGray = 0x9cf09c;
const int cBlueGray = 0xff9c9c;

void _ChessBoard::DrawCustom ()
  {
    int dx, dy;
    _Rect Square;
    int x, y, Pass;
    int c;
    char p_ [] = "?";
    _Font *FontTag;
    //
    dx = Rect.Width / 8;
    dy = Rect.Height / 8;
    FontTag = Parent->FontFind ();   // Use the Forms "normal" font for Tags
    if (FontSet ("CHEQ_TT.TTF", dy * 4 / 5))   // fonts4free.net/chess-font.html
      for (Pass = 0; Pass < 2; Pass++)
        {
          for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++)
              {
                Square = {x * dx, (7 - y) * dy, dx, dy};
                c = Colours [(x ^ y) & 1];
                if (MoveStart)
                  if ((x == MoveFrom.x && y == MoveFrom.y) || (x == MoveTo.x && y == MoveTo.y))
                    c = cWhite;
                if (Pass == 0)
                  DrawRectangle (Square, cBlack, cBlack, c);
                else   // Pass == 1
                  {
                    Square.x++;
                    Square.Width--;
                    if (dy > 40 && FontTag)
                      {
                        FontTag->ColourBG = c;
                        if (x == 0)
                          {
                            p_ [0] = y + '1';
                            TextOutAligned (FontTag, Square, p_, aLeft, aCenter);
                          }
                        if (y == 0)
                          {
                            p_ [0] = x + 'a';
                            TextOutAligned (FontTag, Square, p_, aCenter, aRight);
                          }
                      }
                    Font->ColourBG = c;
                    p_ [0] = PieceToChar (Board [x][y]);
                    TextOutAligned (Font, Square, p_, aCenter, aCenter);
                    /* //Debug - testing pieces attributes
                    if (Board [x][y] & pChecked)
                      DrawLine (Square.x, Square.y, Square.x + dx, Square.y + dy, cRed, 1);
                    if ((int) Board [x][y] / pMoves > 0)
                      DrawLine (Square.x + dx, Square.y, Square.x, Square.y + dy, cBlue, 1);
                    */
                  }
              }
          if (Pass == 0)   // Draw PC move "arrow"
            {
              c = Max (1, dx / 16);   // Width
              if (PCFrom.x >= 0)
                DrawLine (PCFrom.x * dx + dx / 2, (7 - PCFrom.y) * dy + dy / 2, PCTo.x * dx + dx / 2, (7 - PCTo.y) * dy + dy / 2, cGreenGray, c);
              if (MoveFrom.x >= 0)
                DrawLine (MoveFrom.x * dx + dx / 2, (7 - MoveFrom.y) * dy + dy / 2, MoveTo.x * dx + dx / 2, (7 - MoveTo.y) * dy + dy / 2, cBlueGray, c);
            }
        }
  }

bool _ChessBoard::ProcessEventCustom (_Event *Event, _Point Offset)
  {
    int dx, dy;
    _Coord Sel;
    bool Res;
    //
    Res = false;
    if (IsEventMine (Event, Offset) && !PlayThreadStarted)
      {
        dx = Rect.Width / 8;
        dy = Rect.Height / 8;
        Sel.x = (Event->X - Offset.x) / dx;
        Sel.y = 7 - ((Event->Y - Offset.y) / dy);
        if (Event->Type == etMouseDown && Event->Key == KeyMouseLeft)
          {
            MoveFrom = Sel;
            MoveTo = Sel;
            MoveStart = true;
            Invalidate (true);
            Res = true;
          }
        else if (Event->Type == etMouseMove && MoveStart)
          {
            if (Sel.x != MoveTo.x || Sel.y != MoveTo.y)
              {
                MoveTo = Sel;
                Invalidate (true);
              }
            Res = true;
          }
        else if (Event->Type == etMouseUp && Event->Key == KeyMouseLeft)
          if (MoveStart)
            {
              if (MoveFrom.x == MoveTo.x && MoveFrom.y == MoveTo.y)   // we haven't moved
                MoveStart = false;
              else
                MoveComplete = true;
              Invalidate (true);
              Res = true;
            }
      }
    return Res;
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// PROPERTIES FORM
//
////////////////////////////////////////////////////////////////////////////////////////////////////

_ChessBoard *cBoard;
_Form *fProperties;
_Button *bProperties;
_Label *lDepth;

class _FormProperties: public _Form
  {
    public:
      _FormProperties (char *Title, _Rect Position):
        _Form (Title, Position, waAlwaysOnTop) //waFullScreen | waBorderless) //
          {
          }
      ~_FormProperties (void);
  };

_FormProperties::~_FormProperties (void)
  {
    fProperties = NULL;
    bProperties->Down = false;
    bProperties->Invalidate (true);
  }

void ActionDepth (_Container *Container)
  {
    _ButtonArrow *b;
    char St [16], *s;
    //
    b = (_ButtonArrow *) Container;
    if (b)
      if (!b->Down && !PlayThreadStarted)
        if (b->Direction == dUp)
          DepthPlay = Min (DepthPlay + 1, 9);
        else if (b->Direction == dDown)
          DepthPlay = Max (DepthPlay - 1, 0);
    // update label
    if (lDepth)
      {
        s = St;
        StrCat (&s, "Depth ");
        IntToStr (&s, DepthPlay);
        *s = 0;
        lDepth->TextSet (St);
      }
  }

void ActionComplex (_Container *Container)
  {
    _CheckBox *b;
    //
    b = (_CheckBox *) Container;
    SimpleAnalysis = !b->Down;
  }

void ActionColour (_Container *Container)
  {
    _DropList *dl;
    //
    dl = (_DropList *) Container;
    if (dl->Selected >= 0)
      {
        BoardColour = dl->Selected;
        switch (BoardColour)
          {
            case 0: cBoard->Colours [0] = ColourAdjust (cBrown, 140);
                    cBoard->Colours [1] = ColourAdjust (cBrown, 180);
                    break;
            case 1: cBoard->Colours [0] = 0x549375; //ColourAdjust (cAqua, 80);
                    cBoard->Colours [1] = 0xCEEAEA; //ColourAdjust (cAqua, 60);
                    break;
            case 2: cBoard->Colours [0] = ColourAdjust (cOrange, 90);
                    cBoard->Colours [1] = ColourAdjust (cOrange, 160);
                    break;
            case 3: cBoard->Colours [0] = 0x00C0FF;
                    cBoard->Colours [1] = 0x00FCFF;
          }
      }
    Refresh = true;
  }

void ActionProperties (_Container *Container)
  {
    _Button *b;
    _Label *lColour;
    _DropList *dlColour;
    _ButtonArrow *bDepthUp, *bDepthDown;
    _CheckBox *cbComplex;
    int x, y;
    const int bdr = 4;
    const int Ht = 28;
    //
    b = (_Button *) Container;
    if (b->Down)
      {
        fProperties = new _FormProperties ("Setup", {200, 200, 140, 108});
        fProperties->Container->FontSet ("ARI.ttf", 12);
        x = y = bdr;
        lColour = new _Label (fProperties->Container, {x, y, 64, Ht},"Colour");
        x += 64;
        dlColour = new _DropList (fProperties->Container, {x, y, 64, Ht}, "Oak\tTeal\tSunset\tNeon", ActionColour);
        dlColour->Selected = BoardColour;
        x = bdr;
        y += Ht + bdr + bdr;
        lDepth = new _Label (fProperties->Container, {x, y, 64, Ht}, NULL, aLeft);
        ActionDepth (NULL);
        x += 64;
        bDepthUp = new _ButtonArrow (fProperties->Container, {x, y, Ht, Ht / 2}, NULL, ActionDepth, dUp);
        bDepthDown = new _ButtonArrow (fProperties->Container, {x, y + Ht / 2, Ht, Ht / 2}, NULL, ActionDepth, dDown);
        x = bdr;
        y += Ht + bdr;
        cbComplex = new _CheckBox (fProperties->Container, {x, y, 128, Ht}, "Complex Analysis", ActionComplex);
        cbComplex->Down = !SimpleAnalysis;
      }
    else
      {
        delete fProperties;
        fProperties = NULL;
      }
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MAIN FORM and LOOP
//
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Restart;
bool Undo;
bool PlayForMe;

void ActionUndo (_Container *Container)
  {
    if (!((_Button *) Container)->Down)
      Undo = true;
  }

void ActionPlay (_Container *Container)
  {
    if (!((_Button *) Container)->Down)
      PlayForMe = true;
  }

void ActionRestart (_Container *Container)
  {
    if (!((_Button *) Container)->Down)
      Restart = true;
  }

void ActionQuit (_Container *Container)
  {
    if (!((_Button *) Container)->Down)
      Exit = true;
  }

void ActionMenu (_Container *Container)
  {
    _Menu *Menu;
    //
    Menu = (_Menu *) Container;
    if (!PlayThreadStarted)
    if (Menu->Selected == 0)   // Reload
      Load = true;
    else if (Menu->Selected == 1)   // Save
      Save = true;
    //Menu->SelectionList->Die ();
  }

bool TakePiece (_Coord Pos, char **Log, char *Graveyard)
  {
    _Piece p;
    int l;
    char *g;
    //
    p = (_Piece) Board [Pos.x][Pos.y];
    if (Piece (p) != pEmpty)
      {
        if (Log)
          StrCat (Log, "**");
        if (Graveyard)
          {
            g = StrPos (Graveyard, '\n');
            if (g == NULL)
              {
                Graveyard [0] = '\n';
                Graveyard [1] = 0;
                g = Graveyard;
              }
            if (PieceWhite (p))
              StrInsert (g, PieceToChar (p));
            else
              StrAppend (Graveyard, PieceToChar (p));
          }
        return true;
      }
    return false;
  }

const int LogSize = 2000;

int main_ (int argc, char *argv [])
  {
    _Coord FormSize = {650, 600};
    int BoardWidth = 500;
    int btn = 56;
    _Form *Form;
    _Container *Toolbar;
    _Button *bUndo, *bPlay, *bRestart, *bEdit, *bQuit;
    _Label *lMessage;
    _Label *lGraveyard;
    _Label *lPCStats;
    _Label *lLog;
    _Wait *Wait;
    _MenuPopup *Menu;
    bool PCPlay;
    _Coord PCFrom, PCTo;
    //int PlayScore;
    int x, y;
    char *Log, *sLog;
    char Graveyard [100], GraveyardOld [100];
    char PCStats [100], *pPC;
    _SpecialMove sm;
    _Board BoardOld;
    bool CanUndo;
    char *Name;
    int f;   // file ID
    //
    DebugAddS ("===========Start Chess", Revision);
    BoardInit ();
    PlayerWhite = true;
    PCPlay = false;
    Log = (char *) malloc (LogSize);
    // Make the Main Form
    sLog = Log;   // Build the form Title
    StrCat (&sLog, "Stewy's Chess Programme - ");
    StrCat (&sLog, Revision);
    *sLog = 0;
    Form = new _Form (Log, {100, 100, FormSize.x, FormSize.y}, waResizable);
    sLog = Log;
    Form->Container->FontSet ("ARI.ttf", 14);
    Form->Container->Bitmap = BitmapLoad (Form->Window, "Chess.bmp");
    y = 0;
    // build Toolbar
    Toolbar = new _Container (Form->Container, {0, y, 0, 45});
    Toolbar->Colour = ColourAdjust (cForm1, 80);
    x = 0;
    bProperties = new _Button (Toolbar, {x, 0, btn, 0}, "\e0\nSetup", (_Action) ActionProperties); x += btn;
    bProperties->Toggle = true;
    bUndo = new _Button (Toolbar, {x, 0, btn, 0}, "\e1\nUndo", (_Action) ActionUndo); x += btn;
    bPlay = new _Button (Toolbar, {x, 0, btn, 0}, "\e2\nPlay", (_Action) ActionPlay); x += btn;
    bRestart = new _Button (Toolbar, {x, 0, btn, 0}, "\e3\nRestart", (_Action) ActionRestart); x += btn;
    bEdit = new _Button (Toolbar, {x, 0, btn, 0}, "\e4\nEdit"); x += btn;
    bEdit->Toggle = true;
    bQuit = new _Button (Toolbar, {x, 0, btn, 0}, "\e5\nQuit", (_Action) ActionQuit); x += btn;
    y += Toolbar->Rect.Height;
    lMessage = new _Label (Toolbar, {x, 0, 0, 0}, NULL, aCenter);
    lMessage->FontSet (NULL, 30, fsBold);
    lMessage->Colour = cRed;
    lMessage->VisibleSet (false);
    // Dead Pieces go to the Graveyard
    lGraveyard = new _Label (Form->Container, {0, y, BoardWidth, 60}, NULL, aLeft);
    lGraveyard->FontSet ("CHEQ_TT.TTF", lGraveyard->Rect.Height / 2 - 1);
    lGraveyard->RectLock |= rlRight;
    y += lGraveyard->Rect.Height;
    //
    x = 0;
    cBoard = new _ChessBoard (Form->Container, {x, y, BoardWidth, Form->Container->Rect.Height - y - 20});
    cBoard->RectLock |= rlRight + rlBottom;
    //cBoard->Colour = cGray;
    y + cBoard->Rect.Height;
    x += BoardWidth;
    //
    lLog = new _Label (Form->Container, {x + 8, Toolbar->Rect.Height, 0, lGraveyard->Rect.Height + cBoard->Rect.Height - 8}, NULL, aLeft, bNone);
    lLog->RectLock = rlLeft | rlBottom;
    lLog->AlignVert = aRight;   // = Bottom
    lLog->ColourText = ColourAdjust (cAqua, 20); //ColourAdjust (cOrange, 70); cBrown;
    lLog->FontSet ("ARI.ttf", 14, fsBold);
    y += cBoard->Rect.Height;
    //
    lPCStats = new _Label (Form->Container, {0, y, 0, 0}, NULL, aLeft);
    lPCStats->RectLock = rlTop | rlBottom;
    lPCStats->ColourText = ColourAdjust (cGreen, 30);
    //
    Wait = new _Wait (lLog, {0, 0, 0, 0});
    Wait->VisibleSet (false);
    //
    Menu = new _MenuPopup (Form->Container, {}, "Reload\tSave", ActionMenu);
    // Main Loop
    Restart = true;
    PlayForMe = PCPlay = CanUndo = false;
    while (!Exit)
      {
        usleep (1000);
        if (Restart)
          {
            Restart = false;
            MoveCount = 1;
            BoardInit ();
            cBoard->MoveFrom.x = -1;
            cBoard->PCFrom.x = -1;
            Graveyard [0] = 0;
            sLog = Log;
            lMessage->VisibleSet (false);
            lPCStats->TextSet (NULL);
            Refresh = true;
            Undo = CanUndo = false;
          }
        else if (sLog - Log + 50 > LogSize)   // the problem of c strings is ...
          sLog = Log;
        if (Load)
          {
            Load = false;
            Form->Container->EnabledSet (false);
            if (FileSelect (&Name, ".dat", false))
              {
                f = FileOpen (Name, foRead);
                if (f >= 0)
                  {
                    FileRead (f, (byte *) Board, sizeof (Board));
                    FileClose (f);
                    cBoard->MoveFrom.x = -1;
                    cBoard->PCFrom.x = -1;
                    sLog = Log;
                    Refresh = true;
                  }
                free (Name);
              }
            Form->Container->EnabledSet (true);
          }
        if (Save)
          {
            Save = false;
            Form->Container->EnabledSet (false);
            if (FileSelect (&Name, ".dat", true))
              {
                f = FileOpen (Name, foWrite);
                FileWrite (f, (byte *) Board, sizeof (Board));
                FileClose (f);
              }
            Form->Container->EnabledSet (true);
          }
        if (Undo)
          {
            Undo = false;
            if (CanUndo)
              {
                CanUndo = false;
                memcpy (Board, BoardOld, sizeof (_Board));
                cBoard->MoveFrom.x = -1;
                cBoard->PCFrom.x = -1;
                strcpy (Graveyard, GraveyardOld);
                MoveCount--;
                Refresh = true;
              }
          }
        if (PlayThreadFinished)
          {
            PlayThreadFinished = false;
            Toolbar->EnabledSet (true);
            if (PlayThreadWhite == PlayerWhite)   // playing for the human
              if (PlayThreadScore == MININT)
                {
                  lMessage->TextSet ("No Moves, Give up");
                  lMessage->VisibleSet (true);
                }
              else
                {
                  cBoard->MoveFrom = BestA [0];
                  cBoard->MoveTo = BestB [0];
                  cBoard->MoveComplete = true;
                }
            else   // playing for the PC
              {
                if (PlayThreadScore == MININT)
                  {
                    lMessage->TextSet ("I Surrender");
                    lMessage->VisibleSet (true);
                  }
                else
                  {
                    pPC = PCStats;
                    StrCat (&pPC, "PC Move Stats: ");
                    IntToStr (&pPC, MovesConsidered, DigitsCommas);
                    StrCat (&pPC, " moves.  Score ");
                    IntToStr (&pPC, PlayThreadScore, DigitsCommas);
                    StrCat (&pPC, ".  Time ");
                    NumToStrDecimals (&pPC, PlayThreadTime, 3);
                    StrCat (&pPC, " s.  ");
                    /*for (x = 0; x < DepthPlay; x++)
                      {
                        CoordToStr (&pPC, BestA [x]);
                        StrCat (&pPC, '-');
                        CoordToStr (&pPC, BestB [x]);
                        StrCat (&pPC, ' ');
                      }*/
                    *pPC = 0;
                    lPCStats->TextSet (PCStats);
                    PCFrom = BestA [0];
                    PCTo = BestB [0];
                    CoordToStr (&sLog, PCFrom);
                    CoordToStr (&sLog, PCTo);
                    TakePiece (PCTo, &sLog, Graveyard);
                    sm = MovePiece (PCFrom, PCTo);
                    cBoard->PCFrom = PCFrom;
                    cBoard->PCTo = PCTo;
                    cBoard->Invalidate (true);
                    if (sm != smNone)
                      StrCat (&sLog, '+');
                    if (InCheck (PlayerWhite))
                      {
                        lMessage->TextSet ("IN CHECK");
                        lMessage->VisibleSet (true);
                      }
                    else
                      lMessage->VisibleSet (false);
                  }
                MoveCount++;
                StrCat (&sLog, '\n');
                Refresh = true;
              }
            //cBoard->EnabledSet (true);
            Wait->VisibleSet (false);
          }
        if (PCPlay)
          {
            PCPlay = false;
            Toolbar->EnabledSet (false);
            InCheck (!PlayerWhite);   // Mark PC King if in check
            StartThread (PlayThread, (void *) !PlayerWhite);
            Wait->ColourText = ColourAdjust (cGreen, 25);
            Wait->VisibleSet (true);
          }
        if (PlayForMe)
          {
            PlayForMe = false;
            Toolbar->EnabledSet (false);
            StartThread (PlayThread, (void *) PlayerWhite);
            Wait->ColourText = ColourAdjust (cBlue, 50);
            Wait->VisibleSet (true);
          }
        if (cBoard->MoveComplete)
          {
            cBoard->MoveStart = false;
            cBoard->MoveComplete = false;
            if (bEdit->Down)
              {
                TakePiece (cBoard->MoveTo, NULL, Graveyard);
                Board [cBoard->MoveTo.x][cBoard->MoveTo.y] = Board [cBoard->MoveFrom.x][cBoard->MoveFrom.y];
                Board [cBoard->MoveFrom.x][cBoard->MoveFrom.y] = pEmpty;
              }
            else
              {
                memcpy (BoardOld, Board, sizeof (_Board));   // save board for undo
                strcpy (GraveyardOld, Graveyard);
                CanUndo = true;
                StrCat (&sLog, '[');
                IntToStr (&sLog, MoveCount);
                StrCat (&sLog, "] ");
                CoordToStr (&sLog, cBoard->MoveFrom);
                CoordToStr (&sLog, cBoard->MoveTo);
                if (MoveValid (cBoard->MoveFrom, cBoard->MoveTo))
                  {
                    TakePiece (cBoard->MoveTo, &sLog, Graveyard);
                    sm = MovePiece (cBoard->MoveFrom, cBoard->MoveTo);
                    if (sm != smNone)
                      StrCat (&sLog, '+');
                    StrCat (&sLog, ' ');
                    if (InCheck (PlayerWhite))   // You moved into / stayed in Check
                      {
                        memcpy (Board, BoardOld, sizeof (_Board));   // undo move
                        strcpy (Graveyard, GraveyardOld);
                        CanUndo = false;
                        lMessage->TextSet ("Save Your King");
                        lMessage->VisibleSet (true);
                        StrCat (&sLog, " Error\n");
                      }
                    else
                      PCPlay = true;
                  }
                else
                  {
                    StrCat (&sLog, " Error\n");
                    cBoard->MoveFrom.x = -1;
                  }
              }
            Refresh = true;
          }
        if (Refresh)
          {
            Refresh = false;
            cBoard->Invalidate (true);
            lGraveyard->TextSet (Graveyard);
            *sLog = 0;
            lLog->TextSet (Log);
          }
        if (FormsUpdate ())
          break;
      }
    while (FormList)
      delete (FormList);
    free (Log);
    return 0;
  }


/********************************************************************************************************************* JUNK
                Name = (char *) malloc (64);
                sN = Name;
                StrCat (&sN, "board");
                CurrentDateTimeToStr (&sN, true, true);
                StrCat (&sN, ".dat");
                *sN = 0;
*********************************************************************************************************************/
