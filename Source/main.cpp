///////////////////////////////////////////////////////////////////////////////////////////////////
//
// STEWARTS CHESS PROGRAMME
//
// Author: Stewart Tunbridge, Pi Micros
// Email:  stewarttunbridge@gmail.com
// Copyright (c) 2024 Stewart Tunbridge, Pi Micros
//
///////////////////////////////////////////////////////////////////////////////////////////////////


const char Revision [] = "1.3";

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "../Widgets/Widgets.hpp"
#include "../Widgets/WidgetsGrid.hpp"
#include "../Widgets/FileSelect.hpp"
#include "../Widgets/ColourSelect.hpp"
#include "../Widgets/MessageBox.hpp"

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
bool SaveLog;

bool PlayThreadWhite;
bool PlayThreadStarted;
bool PlayThreadFinished;
int PlayThreadScore;
int PlayThreadTime;

int PlayThread (void *PlayWhite)
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
    return 0;
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
      _Coord Move [2];   // Human move
      _Coord MovePC [2];
      bool LegalMovesShow;
      bool LegalMoves [8][8];
      //
      _ChessBoard (_Container *Parent, _Rect Rect);
      ~_ChessBoard ();
      void DrawCustom (void);
      bool ProcessEventCustom (_Event *Event, _Point Offset);
  };

_ChessBoard::_ChessBoard (_Container *Parent, _Rect Rect) : _Container (Parent, Rect)
  {
    Colour = cBlack;
    Colours [0] = ColourAdjust (cBrown, 140);
    Colours [1] = ColourAdjust (cBrown, 180);
    MoveStart = MoveComplete = false;
    Move [0] = Move [1] = {-1, -1};
    MovePC [0] = MovePC [1] = {-1, -1};
    LegalMovesShow = true;
    memset (LegalMoves, false, sizeof (LegalMoves));
  }

_ChessBoard::~_ChessBoard ()
  {
  }

const int cGreenGray = 0x9cf09c;
//const int cBlueGray = 0xff9c9c;
const int cBlueGray = 0xffcccc;

#define CenterX(bx) (bx * dx + dx/2)
#define CenterY(by) ((7-by) * dy + dy/2)

void _ChessBoard::DrawCustom ()
  {
    int dx, dy;
    _Rect Square;
    _Coord Center;
    int x, y, Pass;
    int c;
    char p_ [] = "?";
    _Font *FontTag;
    int cFG, cBG;
    int Width;
    int id;
    //int t;//####
    //
    //t = ClockMS ();
    Time1 = Time2 = Time3 = 0;
    dx = Rect.Width / 8;
    dy = Rect.Height / 8;
    Width = Max (1, dx / 16);
    FontTag = Parent->FontFind ();   // Use the Forms "normal" font for Tags
    if (FontSet ("CHEQ_TT.TTF", dy * 4 / 5))   // fonts4free.net/chess-font.html
      for (Pass = 0; Pass < 2; Pass++)
        {
          for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++)
              {
                Square = {x * dx, (7 - y) * dy, dx, dy};
                Center = {Square.x + dx / 2, Square.y + dy / 2};
                c = Colours [(x ^ y) & 1];
                if (MoveStart)
                  if ((x == Move [0].x && y == Move [0].y) || (x == Move [1].x && y == Move [1].y))
                    c = cWhite;
                if (Pass == 0)
                  DrawRectangle (Square, cBlack, cBlack, c);
                else   // Pass == 1
                  {
                    Square.x++;
                    Square.Width--;
                    if (dy > 40 && FontTag)
                      {
                        cBG = FontTag->ColourBG;
                        cFG = FontTag->Colour;
                        FontTag->ColourBG = c;
                        FontTag->Colour = ColourAdjust (cBlue, 70);
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
                        /*/ Debug
                        int id = Board [x][y] / pMoveID;
                        if (id)
                          {
                            char s [16], *ss = s;
                            //NumToHex (&ss, Board [x][y]);
                            NumToStr (&ss, id);
                            *ss = 0;
                            TextOutAligned (FontTag, Square, s, aRight, aTop);
                          }*/
                        FontTag->ColourBG = cBG;
                        FontTag->Colour = cFG;
                      }
                    Font->ColourBG = c;
                    p_ [0] = PieceToChar (Board [x][y]);
                    TextOutAligned (Font, Square, p_, aCenter, aCenter);
                    if (Board [x][y] & pChecked)
                      DrawCircle (Center.x, Center.y, Max (dx / 16, 1), cRed, ColourAdjust (cRed, 150), 1);
                    if (LegalMovesShow && LegalMoves [x][y])
                      {
                        //DrawRectangle ({x_ - Width, y_ - Width, 2 * Width, 2 * Width}, cBlack, cBlack, cWhite);
                        DrawLine (Center.x - dx / 8, Center.y - dy / 8, Center.x + dx / 8, Center.y + dy / 8, cWhite, Width);
                        DrawLine (Center.x + dx / 8, Center.y - dy / 8, Center.x - dx / 8, Center.y + dy / 8, cWhite, Width);
                      }
                  }
              }
          if (Pass == 0)   // Draw User and PC move "arrows"
            {
              if (MovePC [0].x >= 0)
                {
                  DrawLine (MovePC [0].x * dx + dx / 2, (7 - MovePC [0].y) * dy + dy / 2, MovePC [1].x * dx + dx / 2, (7 - MovePC [1].y) * dy + dy / 2, cGreenGray, Width);
                  DrawCircle (CenterX (MovePC [0].x), CenterY (MovePC [0].y), 2 * Width, -1, cGreenGray, 0);
                }
              if (Move [0].x >= 0)
                {
                  DrawLine (Move [0].x * dx + dx / 2, (7 - Move [0].y) * dy + dy / 2, Move [1].x * dx + dx / 2, (7 - Move [1].y) * dy + dy / 2, cBlueGray, Width);
                  DrawCircle (CenterX (Move [0].x), CenterY (Move [0].y), 2 * Width, -1, cBlueGray, 0);
                }
            }
        }
    //DebugAdd ("Draw Board ", ClockMS () - t);
    //DebugAdd ("Draw Board RenderBitmap time (uS) ", Time1); //####
    //DebugAdd ("Draw Board RenderBitmap make trans masks time (uS) ", Time2); //####
    //DebugAdd ("Draw Board RenderBitmap put up img time (uS) ", Time3); //####
  }

bool _ChessBoard::ProcessEventCustom (_Event *Event, _Point Offset)
  {
    int dx, dy;
    _Coord Sel, Moves [64], *m;
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
            Move [0] = Sel;
            Move [1] = Sel;
            // Store possible moves in MovePos []
            memset (LegalMoves, false, sizeof (LegalMoves));
            if (Piece (Board [Sel.x][Sel.y]) != pEmpty)
              {
                GetPieceMoves (Sel, Moves);
                m = Moves;
                while (m->x >= 0)   // for all moves
                  {
                    LegalMoves [m->x][m->y] = true;
                    m++;
                  }
              }
            MoveStart = true;
            Invalidate (true);
            Res = true;
          }
        else if (Event->Type == etMouseMove && MoveStart)
          {
            if (Sel.x != Move [1].x || Sel.y != Move [1].y)
              {
                Move [1] = Sel;
                Invalidate (true);
              }
            Res = true;
          }
        else if (Event->Type == etMouseUp && Event->Key == KeyMouseLeft)
          if (MoveStart)
            {
              if (Move [0].x == Move [1].x && Move [0].y == Move [1].y)   // we haven't moved
                MoveStart = false;
              else
                MoveComplete = true;
              memset (LegalMoves, false, sizeof (LegalMoves));
              Invalidate (true);
              Res = true;
            }
      }
    return Res;
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// GLOBALS
//
////////////////////////////////////////////////////////////////////////////////////////////////////

_Form *ChessForm;
_Label *lPCStats;


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
      _FormProperties (char *Title, _Point Position):
        _Form (Title, {Position.x, Position.y, 200, 100}, waAlwaysOnTop) //waFullScreen | waBorderless) //
          {
          }
      ~_FormProperties (void);
  };

_FormProperties::~_FormProperties (void)
  {
    fProperties = NULL;
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

_Button *bColour = NULL;

void ActionColourSelected (int Colour)
  {
    if (bColour)
      {
        bColour->Colour = Colour;
        bColour->Invalidate (true);
        if (bColour->DataInt == 1)
          {
            ChessForm->Container->Colour = Colour;
            lPCStats->Colour = Colour;
          }
        else
          {
            ChessForm->Container->ColourGrad = Colour;
            lPCStats->ColourGrad = Colour;
          }
        ChessForm->Container->InvalidateAll (true);
      }
  }

void ActionColourBG (_Container *Container)
  {
    _Button *b;
    //
    b = (_Button *) Container;
    bColour = b;
    if (!b->Down)
      ColourSelect ("Adjust Colour", b->Colour, ActionColourSelected);
  }

void ActionProperties (_Container *Container)
  {
    _Button *b;
    _Label *lColour;
    _DropList *dlColour;
    _Button *bColourBG1, *bColourBG2;
    _ButtonArrow *bDepthUp, *bDepthDown;
    _CheckBox *cbComplex;
    int x, y;
    const int bdr = 4;
    const int Ht = 28;
    //
    b = (_Button *) Container;
    if (!b->Down && fProperties == NULL)
      {
        fProperties = new _FormProperties ("Setup", MousePos ());
        fProperties->Container->FontSet (NULL, 12);
        x = y = bdr;
        lColour = new _Label (fProperties->Container, {x, y, 64, Ht},"Colours");
        x += 64;
        dlColour = new _DropList (fProperties->Container, {x, y, 64, Ht}, "Oak\tTeal\tSunset\tNeon", ActionColour);
        dlColour->Selected = BoardColour;
        x += 64 + 4;
        bColourBG1 = new _Button (fProperties->Container, {x, y, Ht, Ht}, NULL, ActionColourBG); x += Ht + 4;
        bColourBG1->Colour = ChessForm->Container->Colour; bColourBG1->ColourGrad = -1;
        bColourBG1->DataInt = 1;
        bColourBG2 = new _Button (fProperties->Container, {x, y, Ht, Ht}, NULL, ActionColourBG); x += Ht + 4;
        bColourBG2->Colour = ChessForm->Container->ColourGrad; bColourBG2->ColourGrad = -1;
        bColourBG2->DataInt = 2;
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
  }


////////////////////////////////////////////////////////////////////////////////////////////////////
//
// MAIN FORM and LOOP
//
////////////////////////////////////////////////////////////////////////////////////////////////////

bool Restart;
bool Undo;
bool PlayForMe;

char *Help = "\ebStewy's Chess\eb\e5\n"
             "\nDrag a piece to move.\n"
             "Castling, Crowning & En-Passant allowed.\n"
             "\euSetup\eu: Change Colours & Play Level.\n"
             "\euUndo\eu: Take back moves.\n"
             "\euPlay\eu: The PC will move for you.\n"
             "\euRestart\eu: Start again.\n"
             "\euEdit\eu: Move any pieces anywhere.\n"
             "\nRight click for game save etc.\n\n"
             "\eugithub.com/StewartTunbridge\n"
             "www.pimicros.com.au\eu";

typedef struct
  {
    _Coord From;
    _Coord To;
    _Piece OldFrom;
    _Piece OldTo;
    _SpecialMove SpecMov;
  } _UndoItem;

_UndoItem UndoStack [500];

int UndoStackSize;

void GraveyardAddPiece (_Piece Piece, char *Graveyard)
  {
    char *g;
    //
    if (Graveyard)
      {
        g = StrPos (Graveyard, '\n');
        if (g == NULL)
          {
            Graveyard [0] = '\n';
            Graveyard [1] = 0;
            g = Graveyard;
          }
        if (PieceWhite (Piece))
          StrInsert (g, PieceToChar (Piece));
        else
          StrAppend (Graveyard, PieceToChar (Piece));
      }
  }

void GraveyardRemovePiece (_Piece Piece, char *Graveyard)
  {
    char c;
    char *g;
    //
    if (Graveyard)
      {
        c = PieceToChar (Piece);
        g = StrPosLast (Graveyard, c);
        if (g)
          StrDelete (g);
      }
  }

void MovePiece_ (_Coord From, _Coord To, char *Graveyard, char **Logs)
  {
    _UndoItem *ui;
    //
    ui = &UndoStack [UndoStackSize];
    ui->From = From;
    ui->To = To;
    ui->OldFrom = Board [From.x][From.y];
    ui->OldTo = Board [To.x][To.y];
    if (Piece (ui->OldTo) != pEmpty)   // taking piece
      GraveyardAddPiece (ui->OldTo, Graveyard);
    ui->SpecMov = MovePiece (From, To);
    if (ui->SpecMov == smEnPassant)
      GraveyardAddPiece (PieceFrom (pPawn, !PieceWhite (ui->OldFrom)), Graveyard);
    if (Logs)
      {
        if (ui->SpecMov != smNone)
          StrCat (Logs, '+');
        if (Piece (ui->OldTo) != pEmpty || ui->SpecMov == smCrown)
          StrCat (Logs, "**");
      }
    if (UndoStackSize  + 1 < SIZEARRAY (UndoStack))
      UndoStackSize++;
  }

bool UnmovePiece_ (char *Graveyard)
  {
    _UndoItem *ui;
    //
    if (UndoStackSize == 0)
      return false;
    UndoStackSize--;
    ui = &UndoStack [UndoStackSize];
    UnmovePiece (ui->From, ui->To, ui->OldFrom, ui->OldTo, ui->SpecMov);
    if (Piece (ui->OldTo) != pEmpty)   // replacing taken piece
      GraveyardRemovePiece (ui->OldTo, Graveyard);
    if (ui->SpecMov == smEnPassant)
      GraveyardRemovePiece (PieceFrom (pPawn, !PieceWhite (ui->OldFrom)), Graveyard);
    return true;
  }

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

void ActionEdit (_Button *Button)
  {
    cBoard->LegalMovesShow = !Button->Down;
  }

void ActionHelp (_Button *Button)
  {
    if (!Button->Down)
      {
        MessageBox ({400, 400}, Help);
        fMessageBox->Container->Colour = ColourAdjust (cBlue, 160);
        fMessageBox->Container->ColourGrad = cWhite;
      }
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
    else if (Menu->Selected == 2)   // Save Log
      SaveLog = true;
  }

int StrChangeChar (char *p, char c1, char c2)
  {
    int n;
    //
    n = 0;
    while (*p)
      {
        if (*p == c1)
          {
            *p = c2;
            n++;
          }
        p++;
      }
    return n;
  }
bool StrMatch (char **St, char *Target)
  {
    char *p;
    //
    p = *St;
    while (true)
      if (*Target == 0)   // found
        {
          *St = p;
          return true;
        }
      else if (*Target++ != *p++)
        return false;
  }

bool FileWriteLine_ (int f, char *Line)
  {
    bool Res;
    //
    StrChangeChar (Line, '\n', ',');
    Res = FileWriteLine (f, Line);
    StrChangeChar (Line, ',', '\n');
    return Res;
  }

const int LogsSize = 2000;
char *Logs, *sLogs;
char Graveyard [100];
//bool CanUndo;

_Label *lMessage;

void GameSave (char *Name)
  {
    int f;   // file ID
    char Line [LogsSize + 100], *l;
    int x, y;
    //
    f = FileOpen (Name, foWrite);
    if (f >= 0)
      {
        l = Line;
        StrCat (&l, "Board\t");
        for (y = 0; y < 8; y++)
          for (x = 0; x < 8; x++)
            {
              NumToStrBase (&l, Board [x][y], 0, 16);
              StrCat (&l, ',');
            }
        *l = 0;
        FileWriteLine_ (f, Line);
        l = Line;
        StrCat (&l, "Graveyard\t");
        StrCat (&l, Graveyard);
        *l = 0;
        FileWriteLine_ (f, Line);
        l = Line;
        StrCat (&l, "Logs\t");
        StrCat (&l, Logs);
        *l = 0;
        FileWriteLine_ (f, Line);
        l = Line;
        StrCat (&l, "Moves\t");
        NumToStr (&l, MoveCount);
        *l = 0;
        FileWriteLine_ (f, Line);
        FileClose (f);
      }
    else
      StrCat (&sLogs, "*** ERROR SAVING FILE ***");
    //Form->Container->EnabledSet (true);
  }

void GameLoad (char *Name)
  {
    int f;   // file ID
    int Size;
    char *Data, *dp, *dp_;
    int x, y;
    //
    f = FileOpen (Name, foRead);
    if (f >= 0)
      {
        Size = FileSize (f);
        Data = (char *) malloc (Size + 1);
        Data [Size] = 0;   // Terminate String
        FileRead (f, (byte *) Data, Size);
        dp = Data;
        while (*dp)
          {
            // Break off a line
            dp_ = StrPos (dp, '\n');
            *dp_++ = 0;
            // Look for a Data Name
            if (StrMatch (&dp, "Board\t"))
              {
                x = y = 0;
                while (true)
                  {
                    if (*dp == 0)
                      break;
                    Board [x][y] = (_Piece) StrGetHex (&dp);
                    dp++;
                    if (++x == 8)
                      {
                        x = 0;
                        if (++y == 8)
                          break;
                      }
                  }
              }
            else if (StrMatch (&dp, "Graveyard\t"))
              {
                strcpy (Graveyard, dp);
                StrChangeChar (Graveyard, ',', '\n');
              }
            else if (StrMatch (&dp, "Logs\t"))
              {
                strcpy (Logs, dp);
                StrChangeChar (Logs, ',', '\n');
              }
            else if (StrMatch (&dp, "Moves\t"))
              MoveCount = StrGetNum (&dp);
            else
              break;
            dp = dp_;
          }
        FileClose (f);
        free (Data);
        sLogs = StrPos (Logs, (char) 0);
        UndoStackSize = 0;
        cBoard->Move [0].x = -1;
        cBoard->MovePC [0].x = -1;
        lMessage->VisibleSet (false);
        //Form->Container->EnabledSet (true);
        Refresh = true;
      }
    else
      StrCat (&sLogs, "*** ERROR LOADING FILE ***");
  }

void LogSave (char *Name)
  {
    int f;   // file ID
    //
    f = FileOpen (Name, foWrite);
    if (f >= 0)
      {
        FileWrite (f, (byte *) Logs, StrLen (Logs));
        FileClose (f);
      }
    else
      StrCat (&sLogs, "*** ERROR SAVING FILE ***");
  }

int main_ (int argc, char *argv [])
  {
    _Coord FormSize = {650, 600};
    int BoardWidth = 500;
    int btn = 56;
    _Container *Toolbar;
    _Button *bUndo, *bPlay, *bRestart, *bEdit, *bHelp, *bQuit;
    _Label *lGraveyard;
    _Label *lLogs;
    _Wait *Wait;
    _MenuPopup *Menu;
    bool PCPlay;
    _Coord MovePC [2];
    //int PlayScore;
    int x, y;
    char PCStats [100], *pPC;
    _Board BoardOld;
    //
    DebugAddS ("===========Start Chess", Revision);
    BoardInit ();
    PlayerWhite = true;
    PCPlay = false;
    Logs = (char *) malloc (LogsSize);
    // Make the Main Form
    sLogs = Logs;   // Build the form Title
    StrCat (&sLogs, "Stewy's Chess Programme - ");
    StrCat (&sLogs, Revision);
    *sLogs = 0;
    ChessForm = new _Form (Logs, {100, 100, FormSize.x, FormSize.y}, waResizable);
    sLogs = Logs;
    ChessForm->Container->Colour = ColourAdjust (cOrange, 100);
    ChessForm->Container->ColourGrad = ColourAdjust (cOrange, 80);
    ChessForm->Container->FontSet ("Ubuntu-R.ttf", 14);//, fsNoGrayscale);
    ChessForm->Container->Bitmap = BitmapLoad (ChessForm->Window, "Chess2.bmp");
    y = 0;
    // build Toolbar
    Toolbar = new _Container (ChessForm->Container, {0, y, 0, 45});
    Toolbar->Colour = ColourAdjust (cForm1, 80);
    x = 0;
    bProperties = new _Button (Toolbar, {x, 0, btn, 0}, "\e0\nSetup", (_Action) ActionProperties); x += btn;
    bUndo = new _Button (Toolbar, {x, 0, btn, 0}, "\e1\nUndo", (_Action) ActionUndo); x += btn;
    bPlay = new _Button (Toolbar, {x, 0, btn, 0}, "\e2\nPlay", (_Action) ActionPlay); x += btn;
    bRestart = new _Button (Toolbar, {x, 0, btn, 0}, "\e3\nRestart", (_Action) ActionRestart); x += btn;
    bEdit = new _Button (Toolbar, {x, 0, btn, 0}, "\e4\nEdit", (_Action) ActionEdit); x += btn;
    bEdit->Toggle = true;
    bHelp = new _Button (Toolbar, {x, 0, btn, 0}, "\e5\nHelp", (_Action) ActionHelp); x += btn;
    x += 8;
    bQuit = new _Button (Toolbar, {x, 0, btn, 0}, "\e6\nQuit", (_Action) ActionQuit); x += btn;
    y += Toolbar->Rect.Height;
    lMessage = new _Label (Toolbar, {x, 0, 0, 0}, NULL, aCenter);
    lMessage->FontSet (NULL, 24, fsBold);
    lMessage->Colour = cRed;
    lMessage->VisibleSet (false);
    // Dead Pieces go to the Graveyard
    lGraveyard = new _Label (ChessForm->Container, {0, y, BoardWidth, 60}, NULL, aLeft);
    lGraveyard->FontSet ("CHEQ_TT.TTF", lGraveyard->Rect.Height / 2 - 1);
    lGraveyard->RectLock |= rlRight;
    y += lGraveyard->Rect.Height;
    //
    x = 0;
    cBoard = new _ChessBoard (ChessForm->Container, {x, y, BoardWidth, ChessForm->Container->Rect.Height - y - 20});
    cBoard->RectLock |= rlRight + rlBottom;
    //cBoard->Colour = cGray;
    //y += cBoard->Rect.Height;
    x += BoardWidth;
    //
    lLogs = new _Label (ChessForm->Container, {x + 8, Toolbar->Rect.Height, 0, lGraveyard->Rect.Height + cBoard->Rect.Height - 8}, NULL, aLeft, bNone);
    lLogs->RectLock = rlLeft | rlBottom;
    lLogs->AlignVert = aRight;   // = Bottom
    //lLogs->ColourText = ColourAdjust (cAqua, 20); //ColourAdjust (cOrange, 70); cBrown;
    lLogs->FontSet (NULL, 14, fsBold);
    y += cBoard->Rect.Height;
    //
    lPCStats = new _Label (ChessForm->Container, {0, y, 0, 0}, NULL, aLeft);
    lPCStats->RectLock |= rlTop;
    lPCStats->Colour = ChessForm->Container->Colour;//cYellow;//cOrange;
    lPCStats->ColourGrad = ChessForm->Container->ColourGrad;//cYellow;//cOrange;
    //lPCStats->ColourGrad = cOrange;
    lPCStats->ColourText = ColourAdjust (cGreen, 20);
    //
    Wait = new _Wait (lLogs, {0, 0, 0, 0});
    Wait->VisibleSet (false);
    //
    Menu = new _MenuPopup (ChessForm->Container, {}, "\e7Reload\t\e8Save\t\e8Save Log", ActionMenu);
    // Main Loop
    Restart = true;
    PlayForMe = PCPlay = false;
    UndoStackSize = 0;
    while (!Exit)
      {
        usleep (1000);
        //if (PlayThreadStarted)
        //  cBoard->Invalidate (true);
        ChessForm->Container->EnabledSet (!FileSelectActive);
        if (sLogs - Logs + 50 > LogsSize)   // the problem of c strings is ...
          sLogs = Logs;
        if (Restart)
          {
            Restart = false;
            MoveCount = 1;
            BoardInit ();
            cBoard->Move [0].x = -1;
            cBoard->MovePC [0].x = -1;
            Graveyard [0] = 0;
            sLogs = Logs;
            lMessage->VisibleSet (false);
            lPCStats->TextSet (NULL);
            Refresh = true;
            UndoStackSize = 0;
          }
        else if (Load)
          {
            Load = false;
            FileSelect ("Load Game", ".chess", false, GameLoad);
          }
        else if (Save)
          {
            Save = false;
            FileSelect ("Save Game", ".chess", true, GameSave);
          }
        else if (SaveLog)
          {
            SaveLog = false;
            FileSelect ("Save Log", ".log", true, LogSave);
          }
        else if (Undo)
          {
            Undo = false;
            UnmovePiece_(Graveyard);   // undo responce move
            UnmovePiece_(Graveyard);   // undo player move
            cBoard->Move [0].x = -1;
            cBoard->MovePC [0].x = -1;
            MoveCount--;
            Refresh = true;
          }
        else if (PlayThreadFinished)
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
                  cBoard->Move [0] = BestA [0];
                  cBoard->Move [1] = BestB [0];
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
                    DebugAdd (PCStats); //####
                    MovePC [0] = BestA [0];
                    MovePC [1] = BestB [0];
                    CoordToStr (&sLogs, MovePC [0]);
                    CoordToStr (&sLogs, MovePC [1]);
                    //TakePiece (MovePC [1], &sLogs, Graveyard);
                    MovePiece_ (MovePC [0], MovePC [1], Graveyard, &sLogs);
                    cBoard->MovePC [0] = MovePC [0];
                    cBoard->MovePC [1] = MovePC [1];
                    cBoard->Invalidate (true);
                    if (InCheck (PlayerWhite))
                      {
                        lMessage->TextSet ("IN CHECK");
                        lMessage->VisibleSet (true);
                      }
                    else
                      lMessage->VisibleSet (false);
                  }
                MoveCount++;
                StrCat (&sLogs, '\n');
                Refresh = true;
              }
            //cBoard->EnabledSet (true);
            Wait->VisibleSet (false);
          }
        else if (PCPlay)
          {
            PCPlay = false;
            Toolbar->EnabledSet (false);
            InCheck (!PlayerWhite);   // Mark PC King if in check
            StartThread (PlayThread, (void *) !PlayerWhite);
            Wait->ColourText = ColourAdjust (cGreen, 25);
            Wait->VisibleSet (true);
          }
        else if (PlayForMe)
          {
            PlayForMe = false;
            if (!bEdit->Down)   // don't play in Edit mode
              {
                Toolbar->EnabledSet (false);
                StartThread (PlayThread, (void *) PlayerWhite);
                Wait->ColourText = ColourAdjust (cBlue, 50);
                Wait->VisibleSet (true);
              }
          }
        else if (cBoard->MoveComplete)
          {
            cBoard->MoveStart = false;
            cBoard->MoveComplete = false;
            if (bEdit->Down)
              {
                Board [cBoard->Move [1].x][cBoard->Move [1].y] = Board [cBoard->Move [0].x][cBoard->Move [0].y];
                Board [cBoard->Move [0].x][cBoard->Move [0].y] = pEmpty;
                UndoStackSize = 0;
                //MovePiece_ (cBoard->Move [0], cBoard->Move [1], Graveyard, NULL);
              }
            else
              {
                StrCat (&sLogs, '[');
                IntToStr (&sLogs, MoveCount);
                StrCat (&sLogs, "] ");
                CoordToStr (&sLogs, cBoard->Move [0]);
                CoordToStr (&sLogs, cBoard->Move [1]);
                if (MoveValid (cBoard->Move [0], cBoard->Move [1]))
                  {
                    MovePiece_ (cBoard->Move [0], cBoard->Move [1], Graveyard, &sLogs);
                    StrCat (&sLogs, ' ');
                    if (InCheck (PlayerWhite))   // You moved into / stayed in Check
                      {
                        UnmovePiece_ (Graveyard);   // undo move
                        lMessage->TextSet ("Save Your King");
                        lMessage->VisibleSet (true);
                        StrCat (&sLogs, " Error\n");
                      }
                    else
                      PCPlay = true;
                  }
                else
                  {
                    StrCat (&sLogs, " Error\n");
                    cBoard->Move [0].x = -1;
                  }
              }
            Refresh = true;
          }
        if (Refresh)
          {
            Refresh = false;
            cBoard->Invalidate (true);
            lGraveyard->TextSet (Graveyard);
            *sLogs = 0;
            lLogs->TextSet (Logs);
          }
        if (FormsUpdate ())
          break;
      }
    while (FormList)
      delete (FormList);
    free (Logs);
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
