#include <stdio.h>
#include <string.h>
#include <GLUT/glut.h>
#include <OpenGL/OpenGL.h>
#include <math.h>

#define OBJECT_LIST_FILENAME    "objectData"
#define OBJECT_FACES_MAX        40000
#define OBJECT_REGISTER_MAX     30

//プレイヤーオブジェクトはAuxiliary[0]に現在のスピードを保存する。
//オブジェクトに付加できるタグ  ＞＞  "Player", "NPC", "Child", "Environment", "Empty", "Other"

//~~~構造体の宣言~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

typedef struct face {
    double normalVector[3];                             //法線ベクトル
    double vert1[3];                                    //頂点１
    double vert2[3];                                    //頂点２
    double vert3[3];                                    //頂点３
} Face;

typedef struct object {
    int Available;                                      //データが正常かどうか
    int Enable;                                         //画面に描画するか
    char Name[16];                                      //オブジェクトの名前
    char Tag[16];                                       //タグ
    int Child[16];                                      //子オブジェクトの配列番号を格納
    double Position[3];                                 //座標（子オブジェクトはローカル座標)
    double Rotation[3];                                 //回転（子オブジェクトはローカル座標)
    double Auxiliary[5];                                //補助
    int Faces;                                          //ポリゴン数
    Face Polygon[OBJECT_FACES_MAX];                     //ポリゴン
} Object;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


//~~~関数は3つのカテゴリに分けられる~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ここから

//  1 : システム　(初期化、画面の更新を行う)
void Init(void);
void Timer(int v);
void Display(void);
void Resize(int w, int h);
void Mouse(int x, int y);
void keyboard(unsigned char key, int x, int y);

//  2 : ユーティリティ　(ファイルの読み込み、値の変換などを行う関数群)
int ObjectRoader(void);                                 //オブジェクトリストを利用してデータを読み込む
Object ReadSTL(char fileName[]);                        //STLデータを読み込みObject型で返す
void ObjectRenderer(Object obj);                        //引数として受け取ったオブジェクトを描画する
int ObjectRoader(void);                                 //外部からオブジェクトリストを読み込み、配列に格納
double Determinant(double vecA[3], double vecB[3], double vecC[3]);
                                                        //行列演算
double Raycast(double point[3], double ray[3], double vert0[3], double vert1[3], double vert2[3]);
                                                        //レイキャスト（直線と面の交差判定）
double Degree2Radian(double deg);                       //度からラジアンに変換
double Radian2Degree(double rad);                       //ラジアンから度に変換
void LocalMove(Object *obj, double x, double y);        //ローカル座標でオブジェクトを移動
void CameraControl(Object *camera, double xx, double yy, double zz, int gaze, int follow, double speed, double space, Object obj);
                                                        //カメラ(プロジェクション座標)の操作（オブジェクトへの追従など）
void Grid(int qua);                                     //グリッドを描画

//  3 : 制作するゲーム固有のシステム
void OnClick(int x, int y, int xx, int yy);             //クリックした際のイベント
void Loop(void);                                        //メインループ
void CarBehavior(Object *obj, double targetSpeed, double steer, Object *tireRF, Object *tireLF, Object *tireRR, Object *tireLR);    //車の挙動
void TitleView();                                       //タイトル画面
void ResumeView();                                      //一時停止画面
void GameScene();                                       //メイン画面

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ここまで

//========================システム　　　　　　================================ここから

Object objectArray[OBJECT_REGISTER_MAX];                //オブジェクトを格納
double mousePositionRaw[2];                             //ポインタの座標（生データ）
double mousePosition[2];                                //ポインタの座標（スムージング）
char keyInput;                                          //入力されたキー
int keyboardTimer = 0;                                  //キーの長押しに対応するためのタイマ

int main(int argc, char *argv[]){
    glutInitWindowSize(1280,720);
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH);
    glutCreateWindow("Roads");
    Init();
    glutDisplayFunc(Display);
    glutReshapeFunc(Resize);
    glutTimerFunc(100, Timer, 0);
    //glutIdleFunc(Timer);
    glutKeyboardFunc(keyboard);
    glutPassiveMotionFunc(Mouse);
    glutMouseFunc(OnClick);
    glutMainLoop();
    return 0;
}

void Init(void){
    glClearColor(0.7, 0.9, 1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);

    float lightAmbient[4] = {0.7f, 0.9f, 1.0f, 1.0f};
    float lightDiffuse[4] = {0.9f, 0.9f, 1.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);

    ObjectRoader();
}

void Resize(int w, int h){
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(30.0, (double)w / (double)h, 1.0, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

void Timer(int v){
    glutPostRedisplay();
    glutTimerFunc(20, Timer, 0);
}

void Display(void){
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);

    glLoadIdentity();
    
    Loop();

    for(int i = 0; i < OBJECT_REGISTER_MAX; i++){
        ObjectRenderer(objectArray[i]);
    }

    float lightPos[4] = {100.0f, 100.0f, 100.0f, 0.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    glutSwapBuffers();

    if(keyboardTimer < 10){
        keyboardTimer++;
    }else{
        keyInput = '\0';
    }

    double gain = 0.5;
    double contX = 0.0;
    double contY = 0.0;
    contX = gain * (mousePositionRaw[0] - mousePosition[0]);
    mousePosition[0] += contX;
    contY = gain * (mousePositionRaw[1] - mousePosition[1]);
    mousePosition[1] += contY;

}

void keyboard(unsigned char key, int x, int y){
    keyboardTimer = 0;
    keyInput = key;
}

void Mouse(int x, int y){
    if(x >= 0 && x <= 1280 && y >= 0 && y <= 720){
        mousePositionRaw[0] = x;
        mousePositionRaw[1] = y;
    }
}

//========================システム　　　　　　================================ここまで

//========================ユーティリティ　　　================================ここから

int ObjectRoader(void){
    FILE *file;
    file = fopen(OBJECT_LIST_FILENAME,"r");
    if(file == NULL){
        printf("[ERROR] ObjectList can't be read.\n");
        return -1;
    }

    char fileLine[256]; 
    int elem = 0;
    char enr;
    for(;;){
        if((fgets(fileLine, 256, file)) == NULL) break;
        if(strncmp(fileLine, "ObjectName", 10) == 0){
            elem++;
        }
    }
    if(elem > OBJECT_REGISTER_MAX){
        printf("[ERROR] Too many registered object. (Max : %d Detect : %d)\n", OBJECT_REGISTER_MAX, elem);
        return -1;
    }

    printf("[INFO] Successful reading of ObjectList\n");
    printf("--- Reading an object from here ---\n");

    rewind(file);
    int i = 0;
    int children = 0;
    for(;;){
        if((fgets(fileLine, 256, file)) == NULL) break;
        char fileName[16];
        char objectNameTmp[16];
        char tagTmp[16];
        int childTmp[16];
        double PositionTmp[3];
        double RotationTmp[3];

        if(strncmp(fileLine, "ObjectName", 10) == 0){
            sscanf(fileLine, "ObjectName : %s", objectNameTmp);
        }
        if(strncmp(fileLine, "FileName", 8) == 0){
            sscanf(fileLine, "FileName : %s", fileName);
        }
        if(strncmp(fileLine, "Tag", 3) == 0){
            sscanf(fileLine, "Tag : %s", tagTmp);
        }
        if(strncmp(fileLine, "Position", 8) == 0){
            sscanf(fileLine, "Position : %lf %lf %lf", &PositionTmp[0], &PositionTmp[1], &PositionTmp[2]);
        }
        if(strncmp(fileLine, "Rotation", 8) == 0){
            sscanf(fileLine, "Rotation : %lf %lf %lf", &RotationTmp[0], &RotationTmp[1], &RotationTmp[2]);
        }
        if(strncmp(fileLine, "Child", 5) == 0){
            sscanf(fileLine, "Child : %d", &childTmp[children]);
            children++;
        }
        if(strncmp(fileLine, "EndObject", 6) == 0){
            if(strcmp(fileName, "None") != 0){
                objectArray[i] = ReadSTL(fileName);
            }else{
                objectArray[i].Available = 1;
            }
            switch(objectArray[i].Available){
                case -2 :
                    printf("[ERROR] %s contains too many faces. (Max : %d)\n", fileName, OBJECT_FACES_MAX);
                    break;
                case -1 :
                    printf("[ERROR] %s can't be read.\n", fileName);
                    break;
                default :
                    objectArray[i].Enable = 1;
                    strcpy(objectArray[i].Name, objectNameTmp);
                    strcpy(objectArray[i].Tag, tagTmp);
                    for(int j = 0; j < 3; j++){
                        objectArray[i].Position[j] = PositionTmp[j];
                        objectArray[i].Rotation[j] = RotationTmp[j];
                    }
                    for(int j = 0; j < 16; j++){
                        objectArray[i].Child[j] = childTmp[j];
                    }
                    if(children == 0){
                        for(int k = 0; k < 16; k++){
                            childTmp[k] = 0;
                        }
                    }
                    printf("[INFO] Successful reading of %s\n", objectNameTmp);
                    i++;
                    children = 0;
                    break;
            }
        }   
    }
    printf("--- Reading of the object is completed. ---\n");
    return 0;
}

//STLファイルの読み込み
Object ReadSTL(char fileName[]){
    Object objTemp;
    char fileLine[256]; 
    int lines = 0;
    int faces = 0;

    FILE *file;
    file = fopen(fileName,"r");
    if(file == NULL){
        objTemp.Available = -1;
        return objTemp;
    }

    char enr;
    while((enr = fgetc(file)) != EOF){
        if(enr == '\n') lines++;
    }
    faces = (lines -2) /7;
    if(faces >= OBJECT_FACES_MAX){
        objTemp.Available = -2;
        return objTemp;
    } 

    rewind(file);
    int faceCount = 0;
    int vertCount = 0;
    for(;;){
        if((fgets(fileLine, 256, file)) == NULL) break;
        
        if(strncmp(fileLine, "facet", 5) == 0){
            sscanf(fileLine, "facet normal %lf %lf %lf", &objTemp.Polygon[faceCount].normalVector[0], &objTemp.Polygon[faceCount].normalVector[1], &objTemp.Polygon[faceCount].normalVector[2]);
        }
        if(strncmp(fileLine, "verte", 5) == 0){
            switch(vertCount){
                case 0:
                    sscanf(fileLine, "vertex %lf %lf %lf", &objTemp.Polygon[faceCount].vert1[0], &objTemp.Polygon[faceCount].vert1[1], &objTemp.Polygon[faceCount].vert1[2]);
                    vertCount++;
                    break;
                case 1:
                    sscanf(fileLine, "vertex %lf %lf %lf", &objTemp.Polygon[faceCount].vert2[0], &objTemp.Polygon[faceCount].vert2[1], &objTemp.Polygon[faceCount].vert2[2]);
                    vertCount++;
                    break;
                case 2:
                    sscanf(fileLine, "vertex %lf %lf %lf", &objTemp.Polygon[faceCount].vert3[0], &objTemp.Polygon[faceCount].vert3[1], &objTemp.Polygon[faceCount].vert3[2]);
                    vertCount = 0;
                    break;
            }
        }
        if(strncmp(fileLine, "endfa", 5) == 0) faceCount++;
    }
    fclose(file);

    objTemp.Available = 1;
    objTemp.Faces = faces;
    return objTemp;
}

double Determinant(double vecA[3], double vecB[3], double vecC[3]){
    double det = ((vecA[0] * vecB[1] * vecC[2]) + (vecA[1] * vecB[2] * vecC[0]) + (vecA[2] * vecB[0] * vecC[1]) - (vecA[0] * vecB[2] * vecC[1]) - (vecA[1] * vecB[0] * vecC[2]) - (vecA[2] * vecB[1] * vecC[0]));
    return det;
}

double Raycast(double point[3], double ray[3], double vert0[3], double vert1[3], double vert2[3]){
    double invertRay[3] = {};
    double edge1[3] = {};
    double edge2[3] = {};

    for(int i = 0; i < 3; i++){
        invertRay[i] = ray[i] * -1.0;
    }
    for(int i = 0; i < 3; i++){
        edge1[i] = vert1[i] - vert0[i];
        edge2[i] = vert2[i] - vert0[i];
    }

    double denominator = Determinant(edge1, edge2, invertRay);
    //printf("%lf\n",denominator);
    if(denominator <= 0)return 0.0;

    double dd[3] = {};
    double uu, vv, tt;
    for(int i = 0; i < 3; i++){
        dd[i] = point[i] - vert0[i];
    }
    uu = Determinant(dd, edge2, invertRay) / denominator;
    if(uu >= 0 && uu <= 1){
        vv = Determinant(edge1, dd, invertRay) / denominator;
        if(vv >= 0 && uu + vv <= 1){
            tt = Determinant(edge1, edge2, dd) / denominator;
            //printf("%lf\n",tt);
            if(tt < 0)return 0.0;
            return tt;
        }
    }
    return 0.0;
}

void CameraControl(Object *camera, double xx, double yy, double zz, int gaze, int follow, double height, double space, Object obj){
    static double followSpeed = 0.1;
    static double verticalSpeed = 0.1;
    const double gain = 0.01;
    const double vGain = 0.1;

    if(gaze == 1){
        if(follow == 1){
            double targetDistance = sqrt(((camera->Position[0] - obj.Position[0]) * (camera->Position[0] - obj.Position[0]) + (camera->Position[1] - obj.Position[1]) * (camera->Position[1] - obj.Position[1]))
                                *((camera->Position[0] - obj.Position[0]) * (camera->Position[0] - obj.Position[0]) + (camera->Position[1] - obj.Position[1]) * (camera->Position[1] - obj.Position[1])));

            double targetAngle = atan2(obj.Position[1] -camera->Position[1], obj.Position[0] -camera->Position[0]);

            followSpeed = gain *(targetDistance - space);
            verticalSpeed = vGain *(obj.Position[2] + height - camera->Position[2]);

            camera->Position[0] += followSpeed *cos(targetAngle);
            camera->Position[1] += followSpeed *sin(targetAngle);
            camera->Position[2] += verticalSpeed;

            gluLookAt(camera->Position[0], camera->Position[1], camera->Position[2], obj.Position[0], obj.Position[1], obj.Position[2], 0.0, 0.0, 1.0);
        }else{
            gluLookAt(camera->Position[0], camera->Position[1], camera->Position[2], obj.Position[0], obj.Position[1], obj.Position[2], 0.0, 0.0, 1.0);
        }
    }else{
        gluLookAt(camera->Position[0], camera->Position[1], camera->Position[2], xx, yy, zz, 0.0, 0.0, 1.0);
    }
}

double Degree2Radian(double deg){
    double rad = 0;
    rad = deg * M_PI / 180;
    return rad;
}

double Radian2Degree(double rad){
    double deg = 0;
    deg = ( rad / (2 * M_PI) ) * 360;
    return deg;
}

void LocalMove(Object *obj, double x, double y){
    double rad = Degree2Radian((obj->Rotation[2])+90);
    obj->Position[0] += (-1 * y * cos(rad));
    obj->Position[1] += (-1 * y * sin(rad));
}

void ObjectRenderer(Object obj){
    if(obj.Enable != 1)return;
    if(strcmp(obj.Tag, "Child") == 0)return;
    if(strcmp(obj.Tag, "Empty") == 0)return;

    glPushMatrix();

    glTranslated(obj.Position[0], obj.Position[1], obj.Position[2]);

    glRotated(obj.Rotation[2], 0.0, 0.0, 1.0);
    glRotated(obj.Rotation[1], 0.0, 1.0, 0.0);
    glRotated(obj.Rotation[0], 1.0, 0.0, 0.0);

    glBegin(GL_TRIANGLES);
    for(int i = 0; i < obj.Faces; i++){
        glNormal3dv(obj.Polygon[i].normalVector);
        glVertex3dv(obj.Polygon[i].vert1);
        glVertex3dv(obj.Polygon[i].vert2);
        glVertex3dv(obj.Polygon[i].vert3);
    }
    glEnd();

    for(int i = 0; i < 16; i++){
        if(obj.Child[i] >= 1 && obj.Child[i] < OBJECT_REGISTER_MAX ){
            glPushMatrix();
            glTranslated(objectArray[obj.Child[i]].Position[0], objectArray[obj.Child[i]].Position[1], objectArray[obj.Child[i]].Position[2]);

            glRotated(objectArray[obj.Child[i]].Rotation[2], 0.0, 0.0, 1.0);
            glRotated(objectArray[obj.Child[i]].Rotation[1], 0.0, 1.0, 0.0);
            glRotated(objectArray[obj.Child[i]].Rotation[0], 1.0, 0.0, 0.0);

            glBegin(GL_TRIANGLES);
            for(int j = 0; j < objectArray[obj.Child[i]].Faces; j++){
                glNormal3dv(objectArray[obj.Child[i]].Polygon[j].normalVector);
                glVertex3dv(objectArray[obj.Child[i]].Polygon[j].vert1);
                glVertex3dv(objectArray[obj.Child[i]].Polygon[j].vert2);
                glVertex3dv(objectArray[obj.Child[i]].Polygon[j].vert3);
            }
            glEnd();
            glPopMatrix();
        }
    }

    glPopMatrix();
}
//double rayPos[2][3] = {};
void Grid(int qua){
    glPushMatrix();
    glBegin(GL_LINES);
    for(int i = -qua; i <= qua; i++){
        glVertex3d(i, -qua, 0);
        glVertex3d(i, qua, 0);
    }
    for(int i = -qua; i <= qua; i++){
        glVertex3d(-qua, i, 0);
        glVertex3d(qua, i, 0);
    }
    /*glVertex3dv(rayPos[0]);
    rayPos[0][2] = 0.0;
    glVertex3dv(rayPos[0]);
    glVertex3dv(rayPos[1]);
    rayPos[1][2] = 0.0;
    glVertex3dv(rayPos[1]);*/
    glEnd();
    glPopMatrix();
}

//========================ユーティリティ　　　================================ここまで

//========================ゲーム固有のシステム================================ここから

int sceneState = 0;                                     //画面遷移に利用

void OnClick(int x, int y, int xx, int yy){
    if(sceneState == 0 && y == 1){
        objectArray[5].Position[0] = 0.0;
        objectArray[7].Enable = 0;
        sceneState = 1;
    }
    if(sceneState == 2 && y == 1){
        if(xx > 470 && yy > 520 && xx < 830 && yy < 610){
            objectArray[8].Enable = 0;
            objectArray[9].Enable = 0;
            objectArray[10].Enable = 0;
            sceneState = 1;
        }
        if(xx > 510 && yy > 420 && xx < 800 && yy < 510){
            objectArray[7].Enable = 1;
            sceneState = 0;
        }
    }
}

void Loop(void){
    if(keyInput == 'e' && sceneState == 1)sceneState = 2;
    switch(sceneState){
        case 0:
            objectArray[8].Enable = 0;
            objectArray[9].Enable = 0;
            objectArray[10].Enable = 0;
            objectArray[0].Position[0] = 0.0;
            objectArray[0].Position[1] = 0.0;
            objectArray[0].Rotation[2] = 0.0;
            objectArray[0].Auxiliary[0] = 0.0;
            TitleView();
            break;
        case 1:
            GameScene();
            break;
        case 2:
            objectArray[8].Enable = 1;
            objectArray[9].Enable = 1;
            objectArray[10].Enable = 1;
            ResumeView();
            break;
    }
}

void TitleView(void){
    static double rotateTitle = 0.0;

    objectArray[5].Position[0] = sin(Degree2Radian(rotateTitle)) * 5;
    objectArray[5].Position[1] = cos(Degree2Radian(rotateTitle)) * 5;
    
    objectArray[5].Position[2] = 2.0;
    CameraControl(&objectArray[5], 0, 0, 0, 1, 1, 0.8, 10.0, objectArray[7]);

    objectArray[7].Rotation[0] = (mousePosition[1] - 360) /9 * -16.0 /50;
    objectArray[7].Rotation[2] = (mousePosition[0] - 640) * -1.0 /50 +180;

    //objectArray[7].Rotation[2] += 1.0;
    if(rotateTitle > 360)rotateTitle = 0;
    rotateTitle += 0.1;
}

void ResumeView(void){
    objectArray[5].Position[0] = 0;
    objectArray[5].Position[1] = 10;
    
    objectArray[5].Position[2] = 1.0;
    CameraControl(&objectArray[5], 0.0 - (mousePosition[0] - 640) / 1000, 0.0, 3.0 - (mousePosition[1] - 360) /1000, 0, 1, 0.8, 10.0, objectArray[8]);

    objectArray[9].Rotation[0] = (mousePosition[1] - 360) /9 * -16.0 /50;
    objectArray[9].Rotation[2] = (mousePosition[0] - 640) * -1.0 /50 +180;
    objectArray[10].Rotation[0] = (mousePosition[1] - 360) /9 * -16.0 /50;
    objectArray[10].Rotation[2] = (mousePosition[0] - 640) * -1.0 /50 +180;

    objectArray[8].Rotation[2] += 1.0;
    if(objectArray[8].Rotation[2] > 360)objectArray[8].Rotation[2] = 0;
}

void GameScene(void){
    static double playerCarSpeed = 0.0;

    CameraControl(&objectArray[5], 0, 0, 0, 1, 1, 0.8, 10.0, objectArray[0]);
    
    Grid(100);

    double tSpeed;
    switch(keyInput){
        case 'w':
            tSpeed = 0.5;
            break;
        case 's':
            tSpeed = -0.5;
            break;
        default:
            tSpeed = 0;
            break;
    }

    double steer = (mousePosition[0] - 640) / 2;
    CarBehavior(&objectArray[0], tSpeed, steer, &objectArray[3], &objectArray[4], &objectArray[2], &objectArray[1]);
}

void CarBehavior(Object *obj, double targetSpeed, double steer, Object *tireRF, Object *tireLF, Object *tireRR, Object *tireLR){
    static double lastSpeed = 0.0;
    double rollGain = 0.02;
    double accel = 0.0;
    double pitchGain = 0.1;
    double pitch = 0.0;
    double tireRotate = 0.0;
    double zPos = 0.0;
    double zPosGainUp = 1.0;
    double zPosGainDown = 0.2;

    accel = rollGain *(targetSpeed - obj->Auxiliary[0]);
    obj->Auxiliary[0] += accel;

    if(obj->Auxiliary[0] >= 0){
        obj->Rotation[2] += (-0.2 * steer * fabs(obj->Auxiliary[0])) / (fabs(obj->Auxiliary[0]) * 10.0 + 1.0);
        obj->Rotation[1] = steer * obj->Auxiliary[0] * 0.1;
    }else{
        obj->Rotation[2] += (0.2 * steer * fabs(obj->Auxiliary[0])) / (fabs(obj->Auxiliary[0]) * 10.0 + 1.0);
        obj->Rotation[1] = steer * obj->Auxiliary[0] * -0.1;
    }
    //printf("%lf\n",obj->Auxiliary[0]);

    tireRF->Rotation[2] = (steer /6.0) * -1.0;
    tireLF->Rotation[2] = ((steer /6.0) + 180.0) * -1.0;

    tireRotate = obj->Auxiliary[0] * 200;
    tireLF->Rotation[0] -= tireRotate;
    tireRF->Rotation[0] -= tireRotate * -1.0;
    tireLR->Rotation[0] -= tireRotate * -1.0;
    tireRR->Rotation[0] -= tireRotate;
    if(tireLF->Rotation[0] > 360)tireRotate = 0;

    pitch = pitchGain *(((obj->Auxiliary[0] - lastSpeed) * -300.0) - obj->Rotation[0]);
    obj->Rotation[0] += pitch;
    lastSpeed = obj->Auxiliary[0];

    LocalMove(obj, 0, obj->Auxiliary[0]);

    double rayDir[3] = {0.0, 0.0, -1.0};

    double rayPos[2][3] = {};
    rayPos[0][0] = sin(-1.0 * Degree2Radian(objectArray[0].Rotation[2])) + objectArray[0].Position[0];
    rayPos[0][1] = cos(-1.0 * Degree2Radian(objectArray[0].Rotation[2])) + objectArray[0].Position[1];
    rayPos[0][2] = 50.0;
    rayPos[1][0] = sin(-1.0 * Degree2Radian(objectArray[0].Rotation[2] + 180)) + objectArray[0].Position[0];
    rayPos[1][1] = cos(-1.0 * Degree2Radian(objectArray[0].Rotation[2] + 180)) + objectArray[0].Position[1];
    rayPos[1][2] = 50.0;

    double dis[2] = {};
    for(int j = 0; j < 2; j++){
        for(int i = 0; i < objectArray[6].Faces; i++){
            dis[j] = Raycast(rayPos[j], rayDir, objectArray[6].Polygon[i].vert1, objectArray[6].Polygon[i].vert2, objectArray[6].Polygon[i].vert3);
            if(dis[j] > 0.0)break;
        }
    }
    //printf("%lf, %lf\n",dis[1] - dis[0],rayPos[1][1]);
    double ground = atan2(2.0, dis[0] - dis[1]);
    obj->Rotation[0] = Radian2Degree(ground) - 90;

    objectArray[0].Position[2] = 50.2 - dis[1];
    
}

//========================ゲーム固有のシステム================================ここまで