#include "mainwindow.h"
#include "ui_mainwindow.h"




MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow)
{
  ui->setupUi(this);
  connect(ui->restart,SIGNAL(clicked(bool)),this,SLOT(restartGame()));
  connect(ui->regret,SIGNAL(clicked(bool)),this,SLOT(regret()));
  for(int i=0;i<boxNum+1;++i){
      for(int j=0;j<boxNum+1;++j){
          gomoku[i][j]=0;
          gomokuForPaint[i][j]=0;
        }
    }
  nowWhite=false;//true;
  gameOver=false;
  steps=0;
  debug=false;
  AIIsThinking=false;
  searchDepth=4;
  stepAlreadyMade=0;
  initPattern();
  //初始化Zobrist
  zobristHash=0;
  scoreCache.clear();
  std::default_random_engine random(time(NULL));
  std::uniform_int_distribution<int> dis1(0, INT32_MAX);
  for(int i=0;i<15;++i){
      for(int j=0;j<15;++j){
          zobristWhite[i][j]=dis1(random);
          zobristBlack[i][j]=dis1(random);
        }
    }

  //准备线程
  AI *ai=new AI(this);
  ai->moveToThread(&AIThread);
  connect(&AIThread,SIGNAL(finished()),ai,SLOT(deleteLater()));
  connect(this,SIGNAL(AIShouldMove()),ai,SLOT(getAINextStep()));
  connect(ai,SIGNAL(stepReady()),this,SLOT(handleAIStep()));
  AIThread.start();
}

MainWindow::~MainWindow()
{
  delete ui;
  AIThread.quit();
  AIThread.wait();
}

void MainWindow::paintEvent(QPaintEvent *e){
  QPainter painter(this);
      // 反走样
      painter.setRenderHint(QPainter::Antialiasing, true);
      // 设置画笔颜色
      QPen tempPen(QColor(0, 160, 230));
      tempPen.setWidth(3);
      //绘制外边框
      painter.setPen(tempPen);
      const int boardOffset=10;
      painter.drawLine(startX-boardOffset,startY-boardOffset,startX+boardOffset+boxWidth,startY-boardOffset);
      painter.drawLine(startX-boardOffset,startY+boxWidth+boardOffset,startX+boardOffset+boxWidth,startY+boardOffset+boxWidth);
      painter.drawLine(startX-boardOffset,startY-boardOffset,startX-boardOffset,startY+boardOffset+boxWidth);
      painter.drawLine(startX+boardOffset+boxWidth,startY-boardOffset,startX+boxWidth+boardOffset,startY+boardOffset+boxWidth);



      //绘制棋盘
      tempPen.setWidth(1);
      painter.setPen(tempPen);
      // 绘制直线

      //横着
      for(int i=0;i<=boxNum;++i){
          painter.drawLine(startX,startY+i*gap,startX+boxWidth,startY+i*gap);
          painter.drawText(startX-27,startY+i*gap+5,QString::number(i));
        }
      //竖着
      for(int j=0;j<=boxNum;++j){
          painter.drawLine(startX+j*gap,startY,startX+j*gap,startY+boxWidth);
          painter.drawText(startX+j*gap-5,startY-15,QString::number(j));
        }

      QPen whitePen(QColor(255,255,255));
      QPen blackPen(QColor(0,0,0));
      QBrush whilteBrush(QBrush(QColor(255,255,255),Qt::SolidPattern));
      QBrush blackBrush(QBrush(QColor(0,0,0),Qt::SolidPattern));
      for(int i=0;i<boxNum+1;++i){
          for(int j=0;j<boxNum+1;++j){
              if(gomokuForPaint[i][j]==1){
                  painter.setPen(whitePen);
                  painter.setBrush(whilteBrush);
                  painter.drawEllipse(QPoint(startX+i*gap,startY+j*gap),(int)(gap*0.35),(int)(gap*0.35));
                }else if(gomokuForPaint[i][j]==2){
                  painter.setPen(blackPen);
                  painter.setBrush(blackBrush);
                  painter.drawEllipse(QPoint(startX+i*gap,startY+j*gap),(int)(gap*0.35),(int)(gap*0.35));
                }
            }
        }
      //绘制落子点
      QPen redPen(QColor(255,0,0));
      painter.setPen(redPen);
      painter.setBrush(Qt::NoBrush);
      if(stepAlreadyMade>0){
          painter.drawRect(startX+gap*XStack[steps]-(int)(gap*0.5),startY+gap*YStack[steps]-(int)(gap*0.5),
                           gap,gap);
        }
}
void MainWindow::mousePressEvent(QMouseEvent *e){
  if(gameOver||AIIsThinking)return;
  //获得当前坐标
  int x=e->x()-(startX-gap/2);
  int y=e->y()-(startY-gap/2);
  if(x<=0)x=0;
  else{
      x/=gap;
    }
  if(y<=0)y=0;
  else{
      y/=gap;
    }
  if(nowWhite)QOUT<<"白子 "<<"x="<<x<<"  y="<<y;else QOUT<<"黑子  "<<"x="<<x<<"  y="<<y;
  if(x>boxNum||y>boxNum)return;

  if(gomoku[x][y]==0){
      gomoku[x][y]=nowWhite?1:2;
      gomokuForPaint[x][y]=nowWhite?1:2;
      zobristHash^=zobristBlack[x][y];
      ++steps;
      ++stepAlreadyMade;
      XStack[stepAlreadyMade]=x;
      YStack[stepAlreadyMade]=y;
    }else
      return;


  if(judge(x,y,nowWhite)){
      gameOver=true;
      if(nowWhite){
          ui->gameStatus->setText(tr("白棋获胜！"));
        }
      else {
          ui->gameStatus->setText(tr("黑棋获胜！"));
        }
//      qDebug()<<"win !!!"<<endl;
      this->repaint();
      AIIsThinking=false;
      return;
    }
  if(stepAlreadyMade>=15*15){
      ui->gameStatus->setText(tr("平局！"));
      return;
    }
  nowWhite=!nowWhite;
  if(nowWhite)
    ui->who->setText(tr("请白棋落子"));
  else
    ui->who->setText(tr("请黑棋落子"));
//  update();
  this->repaint();
  //对手下子完毕
  ui->gameStatus->setText(tr("AI思考中..."));

  //多线程版本代码
  AIIsThinking=true;
  emit AIShouldMove();
  return;
}
void MainWindow::handleAIStep(){
  gomoku[AINextX][AINextY]=1;
  gomokuForPaint[AINextX][AINextY]=1;
  zobristHash^=zobristWhite[AINextX][AINextY];
  ++steps;
  ++stepAlreadyMade;
  XStack[stepAlreadyMade]=AINextX;
  YStack[stepAlreadyMade]=AINextY;
  if(judge(AINextX,AINextY,nowWhite)){
      gameOver=true;
      if(nowWhite){
          ui->gameStatus->setText(tr("白棋获胜！"));
        }
      else {
          ui->gameStatus->setText(tr("黑棋获胜！"));
        }
//      qDebug()<<"win !!!"<<endl;
      this->repaint();
      AIIsThinking=false;
      return;
    }

  nowWhite=!nowWhite;
  if(nowWhite)
    ui->who->setText(tr("请白棋落子"));
  else
    ui->who->setText(tr("请黑棋落子"));
  ui->gameStatus->setText(tr("游戏进行中"));
//  update();
  this->repaint();
  AIIsThinking=false;
  if(stepAlreadyMade>=15*15){
      ui->gameStatus->setText(tr("平局！"));
    }
qDebug()<<"step:"<<stepAlreadyMade;
qDebug()<<"cache size:"<<scoreCache.size();
}


bool MainWindow::judge(int x, int y, bool nowWhite){
  int count=0;
  int flag=nowWhite?1:2;
  //横向
  for(int i= ((x-4)<0?0:x-4);i<=((x+4)>boxNum?boxNum:x+4);++i){
      if(gomoku[i][y]==flag)count++;
      else count=0;
      if(count==5)return true;
    }
  //纵向
  count=0;
  for(int j= ((y-4)<0?0:y-4);j<=((y+4)>boxNum?boxNum:y+4);++j){
      if(gomoku[x][j]==flag)count++;
      else count=0;
      if(count==5)return true;
    }
  //左下到右上
  count=0;
  for(int i= ((x-4)<0?0:x-4);i<=((x+4)>boxNum?boxNum:x+4);++i){
      int yy=y+i-x;

      if(yy<0)continue;
      else if(yy>boxNum)break;

      if(gomoku[i][yy]==flag)count++;
      else count=0;
      if(count==5)return true;
    }
  //左上到右下
  count=0;
  for(int i= ((x-4)<0?0:x-4);i<=((x+4)>boxNum?boxNum:x+4);++i){
      int yy=y+x-i;

      if(yy>boxNum)continue;
      else if(yy<0)break;

      if(gomoku[i][yy]==flag)count++;
      else count=0;
      if(count==5)return true;
    }

  return false;
}

bool MainWindow::judge(int chess[15][15],int x, int y, bool nowWhite){
  int count=0;
  int flag=nowWhite?1:2;
  //横向
  for(int i= ((x-4)<0?0:x-4);i<=((x+4)>boxNum?boxNum:x+4);++i){
      if(chess[i][y]==flag)count++;
      else count=0;
      if(count==5)return true;
    }
  //纵向
  count=0;
  for(int j= ((y-4)<0?0:y-4);j<=((y+4)>boxNum?boxNum:y+4);++j){
      if(chess[x][j]==flag)count++;
      else count=0;
      if(count==5)return true;
    }
  //左下到右上
  count=0;
  for(int i= ((x-4)<0?0:x-4);i<=((x+4)>boxNum?boxNum:x+4);++i){
      int yy=y+i-x;

      if(yy<0)continue;
      else if(yy>boxNum)break;

      if(chess[i][yy]==flag)count++;
      else count=0;
      if(count==5)return true;
    }
  //左上到右下
  count=0;
  for(int i= ((x-4)<0?0:x-4);i<=((x+4)>boxNum?boxNum:x+4);++i){
      int yy=y+x-i;

      if(yy>boxNum)continue;
      else if(yy<0)break;

      if(chess[i][yy]==flag)count++;
      else count=0;
      if(count==5)return true;
    }

  return false;
}

void MainWindow::restartGame(){
  if(AIIsThinking){
      //结束ai进程
      return;
      AIThread.terminate();
      AIThread.wait();
    }
  for(int i=0;i<boxNum+1;++i){
      for(int j=0;j<boxNum+1;++j){
          gomoku[i][j]=0;
          gomokuForPaint[i][j]=0;
        }
    }
  nowWhite=false;
  gameOver=false;
  steps=0;
  stepAlreadyMade=0;
  zobristHash=0;
  if(nowWhite)
    ui->who->setText(tr("请白棋落子"));
  else
    ui->who->setText(tr("请黑棋落子"));
  ui->gameStatus->setText(tr("游戏进行中"));
  update();
}
void MainWindow::regret(){
  if(AIIsThinking)return;
  if(stepAlreadyMade<=0){
      QMessageBox::warning(this,tr("五子棋"),tr("不能悔棋了"));
      return;
    }
  gomoku[XStack[stepAlreadyMade]][YStack[stepAlreadyMade]]=0;
  gomokuForPaint[XStack[stepAlreadyMade]][YStack[stepAlreadyMade]]=0;
  zobristHash^=zobristWhite[XStack[stepAlreadyMade]][YStack[stepAlreadyMade]];
  --stepAlreadyMade;
  gomoku[XStack[stepAlreadyMade]][YStack[stepAlreadyMade]]=0;
  gomokuForPaint[XStack[stepAlreadyMade]][YStack[stepAlreadyMade]]=0;
  zobristHash^=zobristWhite[XStack[stepAlreadyMade]][YStack[stepAlreadyMade]];
  --stepAlreadyMade;
  nowWhite=false;
  gameOver=false;
  if(nowWhite)
    ui->who->setText(tr("请白棋落子"));
  else
    ui->who->setText(tr("请黑棋落子"));

  ui->gameStatus->setText(tr("棋局进行中"));
  steps-=2;
  update();
}

void MainWindow::initPattern(){
  //初始化评分
  pattern temp;
  //长联
  temp.id=patternID::LongLink;
  temp.whiteScore=100000;
  temp.blackScore=-101000;
  temp.blackPattern.push_back("22222");
  temp.whitePattern.push_back("11111");
  patterns.push_back(temp);
  lightPatterns.push_back(temp);
  //活四
  temp.clear();
  temp.whiteScore=10000;
  temp.blackScore=-10100;
  temp.id=patternID::Active4;
  temp.whitePattern.push_back("011110");
  temp.blackPattern.push_back("022220");
  patterns.push_back(temp);
  lightPatterns.push_back(temp);
  //眠四
  temp.clear();
  temp.whiteScore=500;
  temp.blackScore=-500;
  temp.id=patternID::Sleep4;
  temp.whitePattern.push_back("11011");
  temp.whitePattern.push_back("10111");
  temp.whitePattern.push_back("11101");
  temp.whitePattern.push_back("011112");
  temp.whitePattern.push_back("211110");

  temp.blackPattern.push_back("22022");
  temp.blackPattern.push_back("20222");
  temp.blackPattern.push_back("22202");
  temp.blackPattern.push_back("022221");
  temp.blackPattern.push_back("122220");
  patterns.push_back(temp);

  //活三
  temp.clear();
  temp.whiteScore=200;
  temp.blackScore=-200;
  temp.id=patternID::Active3;
  temp.whitePattern.push_back("011100");
  temp.whitePattern.push_back("001110");
  temp.whitePattern.push_back("010110");
  temp.whitePattern.push_back("011010");

  temp.blackPattern.push_back("022200");
  temp.blackPattern.push_back("002220");
  temp.blackPattern.push_back("020220");
  temp.blackPattern.push_back("022020");
  patterns.push_back(temp);
  lightPatterns.push_back(temp);
  //眠三
  temp.clear();
  temp.whiteScore=50;
  temp.blackScore=-50;
  temp.id=patternID::Sleep3;
  temp.whitePattern.push_back("211100");
  temp.whitePattern.push_back("001112");
  temp.whitePattern.push_back("2011102");
  temp.whitePattern.push_back("210110");
  temp.whitePattern.push_back("011012");
  temp.whitePattern.push_back("211010");
  temp.whitePattern.push_back("010112");
  temp.whitePattern.push_back("10011");
  temp.whitePattern.push_back("11001");
  temp.whitePattern.push_back("10101");

  temp.blackPattern.push_back("122200");
  temp.blackPattern.push_back("002221");
  temp.blackPattern.push_back("1022201");
  temp.blackPattern.push_back("120220");
  temp.blackPattern.push_back("022021");
  temp.blackPattern.push_back("122020");
  temp.blackPattern.push_back("020221");
  temp.blackPattern.push_back("20022");
  temp.blackPattern.push_back("22002");
  temp.blackPattern.push_back("20202");
  patterns.push_back(temp);

  //活2
  temp.clear();
  temp.id=patternID::Active2;
  temp.whiteScore=5;
  temp.blackScore=-5;
  temp.whitePattern.push_back("001100");
  temp.whitePattern.push_back("01010");
  temp.whitePattern.push_back("010010");

  temp.blackPattern.push_back("002200");
  temp.blackPattern.push_back("02020");
  temp.blackPattern.push_back("020020");
  patterns.push_back(temp);

  //眠二
  temp.clear();
  temp.id=patternID::Sleep2;
  temp.whiteScore=3;
  temp.blackScore=-3;
  temp.whitePattern.push_back("211000");
  temp.whitePattern.push_back("000112");
  temp.whitePattern.push_back("210100");
  temp.whitePattern.push_back("001012");
  temp.whitePattern.push_back("210010");
  temp.whitePattern.push_back("010012");
  temp.whitePattern.push_back("10001");
  temp.whitePattern.push_back("2010102");
  temp.whitePattern.push_back("2011002");
  temp.whitePattern.push_back("2001102");

  temp.blackPattern.push_back("122000");
  temp.blackPattern.push_back("000221");
  temp.blackPattern.push_back("120200");
  temp.blackPattern.push_back("002021");
  temp.blackPattern.push_back("120020");
  temp.blackPattern.push_back("020021");
  temp.blackPattern.push_back("20002");
  temp.blackPattern.push_back("1020201");
  temp.blackPattern.push_back("1022001");
  temp.blackPattern.push_back("1002201");
  patterns.push_back(temp);

  //死四
  temp.clear();
  temp.id=patternID::Dead4;
  temp.whiteScore=-10;
  temp.blackScore=10;
  temp.whitePattern.push_back("211112");
  temp.blackPattern.push_back("122221");
  patterns.push_back(temp);

  //死三
  temp.clear();
  temp.id=patternID::Dead3;
  temp.whiteScore=-5;
  temp.blackScore=5;
  temp.whitePattern.push_back("21112");
  temp.blackPattern.push_back("12221");
  patterns.push_back(temp);

  //死二
  temp.clear();
  temp.id=patternID::Dead2;
  temp.whiteScore=-5;
  temp.blackScore=5;
  temp.whitePattern.push_back("2112");
  temp.blackPattern.push_back("1221");
  patterns.push_back(temp);

}

void MainWindow::keyPressEvent(QKeyEvent *e){
  //走子

          step s(XStack[stepAlreadyMade],YStack[stepAlreadyMade]);

          int score=0;
          //横向
          QString line;
          for(int i=((s.x-5)>=0?(s.x-5):0);i<=((s.x+5)<=14?(s.x+5):14);++i){
                          line+=QString::number(gomoku[i][s.y]);
                  }
        qDebug()<<"-:"<<line<<endl;
          for(auto &p:lightPatterns){
                          for(auto &wp:p.whitePattern){
                                          score+=p.whiteScore*line.count(wp);
                                  }
                          for(auto &bp:p.blackPattern){
                                          score+=p.blackScore*line.count(bp);
                                  }
                  }
          //纵向
          line.clear();
          for(int i=((s.y-5)>=0?(s.y-5):0);i<=((s.y+5)<=14?(s.y+5):14);++i){
                          line+=QString::number(gomoku[s.x][i]);
                  }
        qDebug()<<"|:"<<line<<endl;
          for(auto &p:lightPatterns){
                          for(auto &wp:p.whitePattern){
                                          score+=p.whiteScore*line.count(wp);
                                  }
                          for(auto &bp:p.blackPattern){
                                          score+=p.blackScore*line.count(bp);
                                  }
                  }
          // \向
          line.clear();
          for(int i=-5;i<=5;++i){
                          if((s.x+i)<0||(s.y+i)<0)continue;
                          if((s.x+i)>14||(s.y+i)>14)break;
                          line+=QString::number(gomoku[s.x+i][s.y+i]);
                  }
        qDebug()<<"/:"<<line<<endl;
          for(auto &p:lightPatterns){
                          for(auto &wp:p.whitePattern){
                                          score+=p.whiteScore*line.count(wp);
                                  }
                          for(auto &bp:p.blackPattern){
                                          score+=p.blackScore*line.count(bp);
                                  }
                  }
          //  /向
          line.clear();
          for(int i=-5;i<=5;++i){
                          if((s.x+i)<0||(s.y-i)>14)continue;
                          if((s.y-i)<0||(s.x+i)>14)break;
                          line+=QString::number(gomoku[s.x+i][s.y-i]);
                  }
        qDebug()<<"\\:"<<line<<endl;
          for(auto &p:lightPatterns){
                          for(auto &wp:p.whitePattern){
                                          score+=p.whiteScore*line.count(wp);
                                  }
                          for(auto &bp:p.blackPattern){
                                          score+=p.blackScore*line.count(bp);
                                  }
                  }
}
////////////////////////////////////////////////////
//              AI Object
////////////////////////////////////////////////////
void AI::getAINextStep(){
  count=0;
  qDebug()<<"started at:"<<QTime::currentTime().toString();
  qDebug()<<"SCORE:"<<alpha_beta(parentWindow->gomoku,0,-99999999,9999999);
  qDebug()<<count<<" chesses searched";
  qDebug()<<"ended at:"<<QTime::currentTime().toString()<<endl;
  emit stepReady();

}
int AI::getChessScore(int chess[15][15],int leftEdge,int rightEdge,int upEdge,int downEdge){
  int score=0;
  //先分析行-
  for(int i=upEdge;i<=downEdge;++i){
      QString line;
      for(int j=leftEdge;j<=rightEdge;++j){
          line+=QString::number(chess[j][i]);
        }
//      if(debug)qDebug()<<"-:"<<line<<endl;
      score+=getLineScore(line);

    }
  //分析列|
  for(int i=leftEdge;i<=rightEdge;++i){
      QString line;
      for(int j=upEdge;j<=downEdge;++j){
          line+=QString::number(chess[i][j]);
        }
//      if(debug)qDebug()<<"|:"<<line<<endl;
      score+=getLineScore(line);

    }
  //分析斜向下\
  //下半部分
  for(int i=upEdge;i<=downEdge;++i){
      QString line;
      for(int j=0;(i+j)<=downEdge&&(j+leftEdge)<=rightEdge;++j){
          line+=QString::number(chess[j+leftEdge][i+j]);
        }
      if(line.size()<5)break;
//      if(debug)qDebug()<<"\\ down:"<<line<<endl;
      score+=getLineScore(line);

    }
  //上半部分
  for(int i=leftEdge+1;i<=rightEdge;++i){
      QString line;
      for(int j=0;(i+j)<=rightEdge&&(upEdge+j)<=downEdge;++j){
          line+=QString::number(chess[i+j][j+upEdge]);
        }
      if(line.size()<5)break;
//      if(debug)qDebug()<<"\\ up:"<<line<<endl;
      score+=getLineScore(line);
    }

  //分析斜向上/
  //上半部分
  for(int i=downEdge;i>=upEdge;--i){
      QString line;
      for(int j=0;(leftEdge+j)<=rightEdge&&(i-j)>=upEdge;++j){
          line+=QString::number(chess[leftEdge+j][i-j]);
        }
      if(line.size()<5)break;
//      if(debug)qDebug()<<"/ up:"<<line<<endl;
      score+=getLineScore(line);
    }

  //下半部分
  for(int i=leftEdge+1;i<=rightEdge;++i){
      QString line;
      for(int j=0;(downEdge-j)>=upEdge&&(i+j)<=rightEdge;++j){
          line+=QString::number(chess[i+j][downEdge-j]);
        }
      if(line.size()<5)break;
//      if(debug)qDebug()<<"/ down:"<<line<<endl;
      score+=getLineScore(line);

    }
  //棋子的位置分计算
  for(int i=leftEdge;i<=rightEdge;++i){
      for(int j=upEdge;j<=downEdge;++j){
          if(chess[i][j]==0)continue;
          if(chess[i][j]==1){
              score+=parentWindow->basicScore[i][j];//电脑白子 加分
              continue;
            }
          if(chess[i][j]==2){
              score-=parentWindow->basicScore[i][j];//对手黑子 减分
              continue;
            }
        }
    }
  return score;
}
int AI::getLineScore(QString line){
  int score=0;
  for(auto &p:parentWindow->patterns){
      for(auto &wp:p.whitePattern){//电脑白子 加分
          score+=p.whiteScore*line.count(wp);
        }
      for(auto &bp:p.blackPattern){//对手黑子 减分
          score+=p.blackScore*line.count(bp);
        }
    }
  return score;
}

std::vector<step> AI::getPossibleSteps(int chess[15][15],bool white,int leftEdge,int rightEdge,int upEdge,int downEdge){
  std::vector<step> possibleSteps;
  for(int i=leftEdge;i<=rightEdge;++i){
      for(int j=upEdge;j<=downEdge;++j){
          if(chess[i][j]==0){
              possibleSteps.push_back(step(i,j,white));
            }
        }
    }
  return possibleSteps;
}

//走法算分,用于排序
void AI::calculateScore(std::vector<step> &possibleSteps, int chess[15][15]){


  for(auto &s:possibleSteps){
      //记录各种模式的数量
      int patternNum[10]={0,0,0,0,0,0,0,0,0,0};

      //走子
      if(s.nowWhite)chess[s.x][s.y]=1;
      else chess[s.x][s.y]=2;

      int score=0;
      //横向
      QString line;
      for(int i=((s.x-5)>=0?(s.x-5):0);i<=((s.x+5)<=14?(s.x+5):14);++i){
          line+=QString::number(chess[i][s.y]);
        }
//      qDebug()<<"-:"<<line<<endl;
      for(auto &p:parentWindow->patterns){
          for(auto &wp:p.whitePattern){
              int c=line.count(wp);
              score+=p.whiteScore*c;
              if(s.nowWhite){
                  patternNum[p.id]+=c;
                }
            }
          for(auto &bp:p.blackPattern){
              int c=line.count(bp);
              score+=p.blackScore*c;
              if(!s.nowWhite){
                  patternNum[p.id]+=c;
                }
            }
        }
      //纵向
      line.clear();
      for(int i=((s.y-5)>=0?(s.y-5):0);i<=((s.y+5)<=14?(s.y+5):14);++i){
          line+=QString::number(chess[s.x][i]);
        }
//      qDebug()<<"|:"<<line<<endl;
      for(auto &p:parentWindow->patterns){
          for(auto &wp:p.whitePattern){
              int c=line.count(wp);
              score+=p.whiteScore*c;
              if(s.nowWhite){
                  patternNum[p.id]+=c;
                }
            }
          for(auto &bp:p.blackPattern){
              int c=line.count(bp);
              score+=p.blackScore*c;
              if(!s.nowWhite){
                  patternNum[p.id]+=c;
                }
            }
        }
      // \向
      line.clear();
      for(int i=-5;i<=5;++i){
          if((s.x+i)<0||(s.y+i)<0)continue;
          if((s.x+i)>14||(s.y+i)>14)break;
          line+=QString::number(chess[s.x+i][s.y+i]);
        }
//      qDebug()<<"\\:"<<line<<endl;
      for(auto &p:parentWindow->patterns){
          for(auto &wp:p.whitePattern){
              int c=line.count(wp);
              score+=p.whiteScore*c;
              if(s.nowWhite){
                  patternNum[p.id]+=c;
                }
            }
          for(auto &bp:p.blackPattern){
              int c=line.count(bp);
              score+=p.blackScore*c;
              if(!s.nowWhite){
                  patternNum[p.id]+=c;
                }
            }
        }
      //  /向
      line.clear();
      for(int i=-5;i<=5;++i){
          if((s.x+i)<0||(s.y-i)>14)continue;
          if((s.y-i)<0||(s.x+i)>14)break;
          line+=QString::number(chess[s.x+i][s.y-i]);
        }
//      qDebug()<<"/:"<<line<<endl;
      for(auto &p:parentWindow->patterns){
          for(auto &wp:p.whitePattern){
              int c=line.count(wp);
              score+=p.whiteScore*c;
              if(s.nowWhite){
                  patternNum[p.id]+=c;
                }
            }
          for(auto &bp:p.blackPattern){
              int c=line.count(bp);
              score+=p.blackScore*c;
              if(!s.nowWhite){
                  patternNum[p.id]+=c;
                }
            }
        }

      //算分
      s.score=score;
      chess[s.x][s.y]=0;//恢复棋盘
      //有必杀棋后面都不用算了
      if(patternNum[patternID::LongLink]>0){//||patternNum[patternID::Active4]>0||patternNum[patternID::Active3]>2){
          auto bestStep=s;
          possibleSteps.clear();
          possibleSteps.push_back(bestStep);
//          qDebug()<<"found a best step"<<endl;
          break;
        }
   }
}
int AI::alpha_beta(int chess[15][15], int depth, int alpha, int beta){
  if(depth==parentWindow->searchDepth){

      ++count;
      //没有缓存的版本
//      int s=getChessScore(chess);
//      return s;

      auto p=parentWindow->scoreCache.find(parentWindow->zobristHash);
      if(p==parentWindow->scoreCache.end()){
          int s=getChessScore(chess);
          parentWindow->scoreCache[parentWindow->zobristHash]=s;
          return s;
        }else{
          return p->second;
        }

    }
  std::vector<step> possibleSteps;
  int score=0;
  //获得所有走法
  if(depth%2==0){//最大层
      if(parentWindow->stepAlreadyMade>18)possibleSteps=getPossibleSteps(chess,true);
      else if(parentWindow->stepAlreadyMade>10){
          possibleSteps=getPossibleSteps(chess,true,
                                         2-(parentWindow->stepAlreadyMade-10)/4,
                                         12+(parentWindow->stepAlreadyMade-10)/4,
                                         2-(parentWindow->stepAlreadyMade-10)/4,
                                         12+(parentWindow->stepAlreadyMade-10)/4);
        }
      else{
          possibleSteps=getPossibleSteps(chess,true,
                                         6-parentWindow->stepAlreadyMade/2.5,
                                         8+parentWindow->stepAlreadyMade/2.5,
                                         6-parentWindow->stepAlreadyMade/2.5,
                                         8+parentWindow->stepAlreadyMade/2.5);
        }


      //对走法进行排序
//      int sX=parentWindow->XStack[parentWindow->stepAlreadyMade];
//      int sY=parentWindow->YStack[parentWindow->stepAlreadyMade];
//      std::sort(possibleSteps.begin(),possibleSteps.end(),[sX,sY](const step& s1, const step& s2)->bool{
//          int d1=sqrt(abs(s1.x-sX)*abs(s1.x-sX)+abs(s1.y-sY)*abs(s1.y-sY));
//          int d2=sqrt(abs(s2.x-sX)*abs(s2.x-sX)+abs(s2.y-sY)*abs(s2.y-sY));
//          return d1<d2;
//        });
      //新排序法
      calculateScore(possibleSteps,parentWindow->gomoku);
      std::sort(possibleSteps.begin(),possibleSteps.end(),[](const step& s1, const step& s2)->bool{
          return s1.score>s2.score;
        });


//      qDebug()<<"now max layer "<<possibleSteps.size()<<"possible steps"<<endl;
      int count=0;
      for(auto &s:possibleSteps){
//          if(depth==0)qDebug()<<"now searching step "<<++count<<endl;
        chess[s.x][s.y]=1;//走子
        parentWindow->zobristHash^=parentWindow->zobristWhite[s.x][s.y];
        ++parentWindow->stepAlreadyMade;
        parentWindow->XStack[parentWindow->stepAlreadyMade]=s.x;
        parentWindow->YStack[parentWindow->stepAlreadyMade]=s.y;
        score=alpha_beta(chess,depth+1,alpha,beta);
        chess[s.x][s.y]=0;//恢复棋盘
        parentWindow->zobristHash^=parentWindow->zobristWhite[s.x][s.y];
        --parentWindow->stepAlreadyMade;

        if(score>alpha){
            alpha=score;
            if(depth==0){
                parentWindow->AINextX=s.x;
                parentWindow->AINextY=s.y;
              }
          }
        if(alpha>=beta){
            return alpha;
          }
        }
    }else{//最小层
      if(parentWindow->stepAlreadyMade>18)possibleSteps=getPossibleSteps(chess,false);
      else if(parentWindow->stepAlreadyMade>10){
          possibleSteps=getPossibleSteps(chess,true,
                                         2-(parentWindow->stepAlreadyMade-10)/4,
                                         12+(parentWindow->stepAlreadyMade-10)/4,
                                         2-(parentWindow->stepAlreadyMade-10)/4,
                                         12+(parentWindow->stepAlreadyMade-10)/4);
        }
      else{
          possibleSteps=getPossibleSteps(chess,false,
                                         6-parentWindow->stepAlreadyMade/2.5,
                                         8+parentWindow->stepAlreadyMade/2.5,
                                         6-parentWindow->stepAlreadyMade/2.5,
                                         8+parentWindow->stepAlreadyMade/2.5);
        }


      //对走法进行排序
//      int sX=parentWindow->XStack[parentWindow->stepAlreadyMade];
//      int sY=parentWindow->YStack[parentWindow->stepAlreadyMade];
//      std::sort(possibleSteps.begin(),possibleSteps.end(),[sX,sY](const step& s1, const step& s2)->bool{
//          int d1=sqrt(abs(s1.x-sX)*abs(s1.x-sX)+abs(s1.y-sY)*abs(s1.y-sY));
//          int d2=sqrt(abs(s2.x-sX)*abs(s2.x-sX)+abs(s2.y-sY)*abs(s2.y-sY));
//          return d1<d2;
//        });
      //新排序法
      calculateScore(possibleSteps,parentWindow->gomoku);
      std::sort(possibleSteps.begin(),possibleSteps.end(),[](const step& s1, const step& s2)->bool{
          return s1.score<s2.score;
        });



      for(auto &s:possibleSteps){
          chess[s.x][s.y]=2;//走子
          parentWindow->zobristHash^=parentWindow->zobristBlack[s.x][s.y];
          ++parentWindow->stepAlreadyMade;
          parentWindow->XStack[parentWindow->stepAlreadyMade]=s.x;
          parentWindow->YStack[parentWindow->stepAlreadyMade]=s.y;
          score=alpha_beta(chess,depth+1,alpha,beta);
          chess[s.x][s.y]=0;//恢复棋盘
          parentWindow->zobristHash^=parentWindow->zobristBlack[s.x][s.y];
          --parentWindow->stepAlreadyMade;
          if(score<beta)beta=score;
          if(alpha>=beta){
              return beta;
            }
        }
    }
  return depth%2==0?alpha:beta;
}
