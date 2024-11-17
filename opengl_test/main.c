/*
Переделать время с glutPostRedisplay(); на собственно сделанное
Сделать, чтобы пули не стреляли бесконечно
Почему звезды перестают появляться - ф-я draw_stars запускается единожды
*/
#include <GL/freeglut.h>
#include <time.h>
#include <stdio.h>
#include <stb_easy_font.h>   //для выведения букв
#include <stdlib.h>

void print_string(float x, float y, char* text, float r, float g, float b) //кусок кода, вытащенный из библиотеки сверху, нужен для печати букв
{																	//
	static char buffer[9000]; // ~500 chars
	int num_quads;

	num_quads = stb_easy_font_print(x, y, text, NULL, buffer, sizeof(buffer));

	glColor3f(r, g, b);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 16, buffer);
	glDrawArrays(GL_QUADS, 0, num_quads * 4);
	glDisableClientState(GL_VERTEX_ARRAY);
}

typedef enum {
	left,
	right,
	no
} side;

typedef struct Object {
	double xCoord;
	double yCoord;
	double speed;
	time_t time_of_create;

	struct Object* Parent;
	side side_kid; // это правый или левый ребенок родителя
	struct Object* pLeft;
	struct Object* pRight;
} Object;

// звезды - фон
#define MAX_STARS 300 // количество звезд, которые могут существовать одновременно
Object stars[MAX_STARS]; // нет необходимости делать дерево, т.к. нет поиска
int num_of_stars = 0;
double speed_of_star = 1.0; // коэффицент изменения скорости звезд
//double stars[MAX_STARS][3]; //первые два значения - координата по x и y, третье - скорость (у каждой она будет своя)

//координаты носа коробля
double xCoord = -65;
double yCoord = 0.0;
double move = 10; //шаг перемещения
double size_of_spaceship = 7;

//количество пуль и координаты каждой из них.
#define MAX_BULLETS 50
int num_of_bullets = 0;
Object* bullets_tree = NULL;
double puli[MAX_BULLETS][2]; //первое - координата по х, второе - по y
double speed_of_bullet = 1.5;
time_t last_shooted_bullet; // время (будет создаваться как clock()) последней выстреленной пули
							// нужно, чтобы сделать так, чтоб пули не летели одним потоком (ограничить количество пуль в ед. времени)


// астероиды - то, что уничтожаем
Object* asteroid_tree = NULL;
int num_of_asteroids = 0;
double speed_of_asteroids = 2.0; // коэффицент изменения скорости астероидов
double size_first_asteroid = 7;
int posibility_of_spawn_asteroids = 30; // меняется в spaceship_move (в будующем может быть в клавиатуре)
/*
#define MAX_ASTEROIDS 100 // количество астероидов, которые могут существовать одновременно
double asteroids[MAX_ASTEROIDS][4]; //первые два значения - координата по x и y, 3 - скорость (у каждой она будет своя), 4 - тип астероида
*/

int difficulty = 0; // 0 - меню выбора сложности, 1 - easy, 2 - medium, 3 - hard, 4 - меню проигрыша
int choose = 1; //нужно для выбора в меню. 1 - подсвечивает easy, 2 - medium, 3 - hard 
int score = 0; // очки
int lives = 3;
time_t last_lost_life; // нужно, чтоб проходило какое-то время после потери жизни. эта переменная будет отсчитывать это время
#define REGENIGATION_TIME 2000 // количество тиков, нужное для того, чтобы жизнь могла отняться повторно

#define MAX_BONUS 10
int num_of_bonus = 0;
Object* bonuses_tree = NULL;
double bonuses[MAX_BONUS][4]; // [3] == 1 - это доп. жизнь
double speed_of_bonus = 1;
double size_of_bonus = 0.4;
int posibility_of_spawn_bonus = 100;
time_t last_taken_bonus; // нужно, чтоб проходило какое-то время после потери жизни. эта переменная будет отсчитывать это время
#define CAN_TAKE_NEW_BONUS 100 // количество тиков, нужное для того, чтобы подобрать новый бонус
double time_x2_bonus = 0; // время действия x2 бонуса

Object* create_new_Object(Object item, Object* parent, side side_kid) {
	Object* p = (Object*)malloc(sizeof(Object));
	p->xCoord = item.xCoord;
	p->yCoord = item.yCoord;
	p->speed = item.speed;

	p->Parent = parent;
	p->side_kid = side_kid;
	p->pLeft = NULL;
	p->pRight = NULL;
	return p;
}

// будем строить дерево по y координате, т.к. по ней потом будем искать
Object* add_to_tree(Object* tree ,Object item) {
	if (tree != NULL) { // сначала наиболее вероятный случай для эффективности
		Object* p = tree;
		while (1) {
			if (p->yCoord >= item.yCoord) {
				if (p->pLeft == NULL) {
					p->pLeft = create_new_Object(item, p, left);
					break;
				}
				p = p->pLeft;
			}
			else {
				if (p->pRight == NULL) {
					p->pRight = create_new_Object(item, p, right);
					break;
				}
				p = p->pRight;
			}
		}
	}
	else {
		tree = create_new_Object(item, NULL, no);
	}
	return tree;
}

// записывает вместо текущих значений, значения самого правого элемента из левого поддерева
// или самого левого элемента из правого поддерева
// затем удаляет этот элемент
// Если это уже самый нижний, то нужно удалить его

void delete_node(Object* asteroid) {
	Object* p;
	if ((asteroid->pLeft == NULL) && (asteroid->pRight == NULL)) { // если лист
		if (asteroid->side_kid == left) asteroid->Parent->pLeft = NULL;
		if (asteroid->side_kid == right) asteroid->Parent->pRight = NULL;
		return;
	}
	else if (asteroid->pLeft == NULL) {
		p = asteroid->pRight;
		int sdf = 0;
		while (p->pLeft != NULL && p->pRight != NULL) {
			sdf++;
			if (p->pLeft != NULL) p = p->pLeft;
			else p = p->pRight;
		}

	}
	else if (asteroid->pRight == NULL) {
		p = asteroid->pLeft;
		while (p->pLeft != NULL && p->pRight != NULL) {
			if (p->pRight != NULL) p = p->pRight;
			else p = p->pLeft;
		}
	}
	else {
		p = asteroid->pLeft;
		while (p->pLeft != NULL && p->pRight != NULL) {
			if (p->pRight != NULL) p = p->pRight;
			else p = p->pLeft;
		}
	}
	//копируем данные
	asteroid->speed = p->speed;
	asteroid->xCoord = p->xCoord;
	asteroid->yCoord = p->yCoord;
	asteroid->time_of_create = p->time_of_create;
	//удаляем листок
	if (p->side_kid == left) p->Parent->pLeft = NULL;
	if (p->side_kid == right) p->Parent->pRight = NULL;
}

void draw_bonuses() {
	
	if (num_of_bonus > MAX_BONUS - 1) num_of_bonus = 0; //когда пуль в памяти более 10000, записываем координаты новых в начало
	for (int i = 0; i < MAX_BONUS; i++) {
		bonuses[i][0] -= bonuses[i][2];
		if (bonuses[i][3] == 1) {
			glBegin(GL_POLYGON);
			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0], bonuses[i][1] - 7 * size_of_bonus, 0);
			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] + 3 * size_of_bonus, bonuses[i][1] - 9 * size_of_bonus, 0);
			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] + 6 * size_of_bonus, bonuses[i][1] - 9 * size_of_bonus, 0);
			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] + 9 * size_of_bonus, bonuses[i][1] - 6 * size_of_bonus, 0);
			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] + 9 * size_of_bonus, bonuses[i][1] - 2 * size_of_bonus, 0);
			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] + 6 * size_of_bonus, bonuses[i][1] + 3 * size_of_bonus, 0);
			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0], bonuses[i][1] + 9 * size_of_bonus, 0);
			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] - 6 * size_of_bonus, bonuses[i][1] + 3 * size_of_bonus, 0);
			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] - 9 * size_of_bonus, bonuses[i][1] - 2 * size_of_bonus, 0);
			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] - 9 * size_of_bonus, bonuses[i][1] - 6 * size_of_bonus, 0);
			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] - 6 * size_of_bonus, bonuses[i][1] - 9 * size_of_bonus, 0);
			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] - 3 * size_of_bonus, bonuses[i][1] - 9 * size_of_bonus, 0);
			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] - 0 * size_of_bonus, bonuses[i][1] - 7 * size_of_bonus, 0);
			glEnd();

			glLineWidth(size_of_bonus * 4);
			glBegin(GL_LINES);
			glColor3f(1, 1, 0); glVertex3f(bonuses[i][0] - 13 * size_of_bonus, bonuses[i][1] + 12 * size_of_bonus, 0);
			glColor3f(1, 1, 0); glVertex3f(bonuses[i][0] - 13 * size_of_bonus, bonuses[i][1] - 12 * size_of_bonus, 0);

			glColor3f(1, 1, 0); glVertex3f(bonuses[i][0] - 12 * size_of_bonus, bonuses[i][1] - 13 * size_of_bonus, 0);
			glColor3f(1, 1, 0); glVertex3f(bonuses[i][0] + 12 * size_of_bonus, bonuses[i][1] - 13 * size_of_bonus, 0);

			glColor3f(1, 1, 0); glVertex3f(bonuses[i][0] + 13 * size_of_bonus, bonuses[i][1] - 12 * size_of_bonus, 0);
			glColor3f(1, 1, 0); glVertex3f(bonuses[i][0] + 13 * size_of_bonus, bonuses[i][1] + 12 * size_of_bonus, 0);

			glColor3f(1, 1, 0); glVertex3f(bonuses[i][0] + 12 * size_of_bonus, bonuses[i][1] + 13 * size_of_bonus, 0);
			glColor3f(1, 1, 0); glVertex3f(bonuses[i][0] - 12 * size_of_bonus, bonuses[i][1] + 13 * size_of_bonus, 0);
			glEnd();

		} // жизни
		
		if (bonuses[i][3] == 2) {
			glLineWidth(size_of_bonus * 8);
			glBegin(GL_LINES);
			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] - 9 * size_of_bonus, bonuses[i][1] - 9 * size_of_bonus, 0); // X начало
			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] - 3 * size_of_bonus, bonuses[i][1] + 9 * size_of_bonus, 0);

			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] - 3 * size_of_bonus, bonuses[i][1] - 9 * size_of_bonus, 0);
			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] - 9 * size_of_bonus, bonuses[i][1] + 9 * size_of_bonus, 0); // Х конец

			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] + 1 * size_of_bonus, bonuses[i][1] - 8 * size_of_bonus, 0);
			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] + 8 * size_of_bonus, bonuses[i][1] - 8 * size_of_bonus, 0);

			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] + 8 * size_of_bonus, bonuses[i][1] - 8 * size_of_bonus, 0);
			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] - 0 * size_of_bonus, bonuses[i][1] + 8 * size_of_bonus, 0);

			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] - 0 * size_of_bonus, bonuses[i][1] + 8 * size_of_bonus, 0);
			glColor3f(1, 0, 0); glVertex3f(bonuses[i][0] + 8 * size_of_bonus, bonuses[i][1] + 8 * size_of_bonus, 0);

			glEnd();


			glLineWidth(size_of_bonus * 4);
			glBegin(GL_LINES);
			glColor3f(1, 1, 0); glVertex3f(bonuses[i][0] - 13 * size_of_bonus, bonuses[i][1] + 12 * size_of_bonus, 0);
			glColor3f(1, 1, 0); glVertex3f(bonuses[i][0] - 13 * size_of_bonus, bonuses[i][1] - 12 * size_of_bonus, 0);

			glColor3f(1, 1, 0); glVertex3f(bonuses[i][0] - 12 * size_of_bonus, bonuses[i][1] - 13 * size_of_bonus, 0);
			glColor3f(1, 1, 0); glVertex3f(bonuses[i][0] + 12 * size_of_bonus, bonuses[i][1] - 13 * size_of_bonus, 0);

			glColor3f(1, 1, 0); glVertex3f(bonuses[i][0] + 13 * size_of_bonus, bonuses[i][1] - 12 * size_of_bonus, 0);
			glColor3f(1, 1, 0); glVertex3f(bonuses[i][0] + 13 * size_of_bonus, bonuses[i][1] + 12 * size_of_bonus, 0);

			glColor3f(1, 1, 0); glVertex3f(bonuses[i][0] + 12 * size_of_bonus, bonuses[i][1] + 13 * size_of_bonus, 0);
			glColor3f(1, 1, 0); glVertex3f(bonuses[i][0] - 12 * size_of_bonus, bonuses[i][1] + 13 * size_of_bonus, 0);
			glEnd();
		}   //доп очки
	}

}

void draw_asteroids(Object* p) {
	if (p == NULL) return;
	draw_asteroids(p->pLeft);
	draw_asteroids(p->pRight);

	p->xCoord -= p->speed;
	
	glBegin(GL_POLYGON);
	glColor3f(0.5, 0.5, 0.5); glVertex3f(p->xCoord - size_first_asteroid, p->yCoord + size_first_asteroid / 2, 1);
	glColor3f(0.5, 0.5, 0.5); glVertex3f(p->xCoord - size_first_asteroid, p->yCoord - size_first_asteroid / 2, 1);
	glColor3f(0.5, 0.5, 0.5); glVertex3f(p->xCoord - size_first_asteroid / 2, p->yCoord - size_first_asteroid, 1);
	glColor3f(0.7, 0.7, 0.7); glVertex3f(p->xCoord + size_first_asteroid / 2, p->yCoord - size_first_asteroid, 1);
	glColor3f(0.7, 0.7, 0.7); glVertex3f(p->xCoord + size_first_asteroid, p->yCoord - size_first_asteroid / 2, 1);
	glColor3f(0.7, 0.7, 0.7); glVertex3f(p->xCoord + size_first_asteroid, p->yCoord + size_first_asteroid / 2, 1);
	glColor3f(0.5, 0.5, 0.5); glVertex3f(p->xCoord + size_first_asteroid / 2, p->yCoord + size_first_asteroid, 1);
	glColor3f(0.5, 0.5, 0.5); glVertex3f(p->xCoord - size_first_asteroid / 2, p->yCoord + size_first_asteroid, 1);
	glEnd();

	glPointSize(size_first_asteroid * 5 / 7);
	glBegin(GL_POINTS);
	glColor3f(0.4, 0.4, 0.4); glVertex3f(p->xCoord - size_first_asteroid / 2, p->yCoord - size_first_asteroid / 4, 0);
	glColor3f(0.35, 0.35, 0.35); glVertex3f(p->xCoord - size_first_asteroid / 4, p->yCoord - size_first_asteroid / 2, 0);
	glColor3f(0.35, 0.35, 0.35); glVertex3f(p->xCoord - size_first_asteroid / 3, p->yCoord + size_first_asteroid / 2, 0);
	glColor3f(0.55, 0.55, 0.55); glVertex3f(p->xCoord + size_first_asteroid / 1.9, p->yCoord + size_first_asteroid / 3, 0);
	glEnd();
}

void draw_stars() { //ф-я которая создаёт звезды на фоне.
	if ((rand() % 10) == 9) {   //выбираем случайное время, при достижении которого генерируется звезда. чем больше значение после %, тем ниже вероятность появления
		double y = ((rand() % 18) * 10) - 75; //выбирается случайное значение высоты для появившейся звезды. 75 (вместо 90) - немного сдвигаем вниз, чтоб не залезали на интерфейс
		stars[num_of_stars].xCoord = 100;  //начальная координата по х. спавним справа от экрана
		stars[num_of_stars].yCoord = y;
		stars[num_of_stars].speed = (speed_of_star * (rand() % 10)) / 10 + 0.1;  // скорость. добавляем константу, чтоб те звезды, у которых скорость выпала 0 тоже двигались.
		num_of_stars++;
	}	
	if (num_of_stars > MAX_STARS - 1) num_of_stars = 0; //когда пуль в памяти более 10000, записываем координаты новых в начало
	for (int i = 0; i < MAX_STARS; i++) {  // - сколько звезд может появиться до того, как на места старых, начнут вставать новые.  
		glLineWidth(5);
		stars[i].xCoord -= stars[i].speed;
		glBegin(GL_LINES);
		glColor3f(0.8, 0.8, 0.8); glVertex3f(stars[i].xCoord, stars[i].yCoord, 0);
		glColor3f(0.08, 0.08, 0.13); glVertex3f(stars[i].xCoord + 8, stars[i].yCoord, 0); // +5 для их растягивания вдоль x
		glEnd();
	}
}

void spaceship() {
	if (((clock() - last_lost_life < REGENIGATION_TIME) && (((clock() % 100) / 10) % 2 == 1))); // если исполняется, кораблик исчезает (начинает моргать)
	else {					// иначе рисуется

		// ОГОНЬ
		glPointSize(size_of_spaceship * 0.5);
		glBegin(GL_POINTS);
		glColor3f(1, 1, 0); glVertex3f(xCoord - 2.7 * size_of_spaceship - (rand() % 3), yCoord - size_of_spaceship + 1 + (rand() % 13), 0);
		glColor3f(1, 1, 0); glVertex3f(xCoord - 2.7 * size_of_spaceship - (rand() % 3), yCoord - size_of_spaceship + 1 + (rand() % 13), 0);
		glColor3f(1, 1, 0); glVertex3f(xCoord - 2.7 * size_of_spaceship - (rand() % 3), yCoord - size_of_spaceship + 1 + (rand() % 13), 0);
		glColor3f(1, 1, 0); glVertex3f(xCoord - 2.7 * size_of_spaceship - (rand() % 3), yCoord - size_of_spaceship + 1 + (rand() % 13), 0);

		glColor3f(1, 0, 0); glVertex3f(xCoord - 2.7 * size_of_spaceship - (rand() % 3), yCoord - size_of_spaceship + 1 + (rand() % 13), 0);
		glColor3f(1, 0, 0); glVertex3f(xCoord - 2.7 * size_of_spaceship - (rand() % 3), yCoord - size_of_spaceship + 1 + (rand() % 13), 0);
		glColor3f(1, 0, 0); glVertex3f(xCoord - 2.7 * size_of_spaceship - (rand() % 3), yCoord - size_of_spaceship + 1 + (rand() % 13), 0);
		glColor3f(1, 0, 0); glVertex3f(xCoord - 2.7 * size_of_spaceship - (rand() % 3), yCoord - size_of_spaceship + 1 + (rand() % 13), 0);
		glEnd();


		// КОРАБЛЬ
		glBegin(GL_POLYGON);
		glColor3f(0.5, 0.5, 0.9); glVertex3f(xCoord, yCoord, 0);
		glColor3f(1, 1, 1); glVertex3f(xCoord - size_of_spaceship, yCoord + size_of_spaceship, 0);
		glColor3f(1, 1, 1); glVertex3f(xCoord - size_of_spaceship * 2.5, yCoord +  0 * size_of_spaceship, 0);
		glColor3f(1, 1, 1); glVertex3f(xCoord - size_of_spaceship, yCoord - size_of_spaceship, 0);
		glEnd();

		glBegin(GL_POLYGON);
		glColor3f(1, 1, 1); glVertex3f(xCoord - 2 * size_of_spaceship, yCoord, 0);
		glColor3f(1, 1, 1); glVertex3f(xCoord - 2.7 * size_of_spaceship, yCoord - size_of_spaceship, 0);
		glColor3f(1, 1, 1); glVertex3f(xCoord - 2.7 * size_of_spaceship, yCoord + size_of_spaceship, 0);
		glEnd();

		glLineWidth(size_of_spaceship * 0.3);
		glBegin(GL_LINES);
		glColor3f(1, 1, 1); glVertex3f(xCoord - 2.7 * size_of_spaceship, yCoord - size_of_spaceship, 0);
		glColor3f(1, 1, 1); glVertex3f(xCoord - 3 * size_of_spaceship, yCoord - size_of_spaceship, 0);

		glColor3f(1, 1, 1); glVertex3f(xCoord - 2.7 * size_of_spaceship, yCoord + size_of_spaceship, 0);
		glColor3f(1, 1, 1); glVertex3f(xCoord - 3 * size_of_spaceship, yCoord + size_of_spaceship, 0);
		glEnd();

		


		/*
		glBegin(GL_POLYGON);
		glColor3f(1, 1, 1); glVertex3f(xCoord, yCoord, 0);
		glColor3f(1, 1, 1); glVertex3f(xCoord - size_of_spaceship, yCoord + size_of_spaceship, 0);
		glColor3f(1, 1, 1); glVertex3f(xCoord - size_of_spaceship * 3, yCoord + size_of_spaceship, 0);
		glColor3f(1, 1, 1); glVertex3f(xCoord - size_of_spaceship * 3, yCoord - size_of_spaceship, 0);
		glColor3f(1, 1, 1); glVertex3f(xCoord - size_of_spaceship, yCoord - size_of_spaceship, 0);
		glEnd();
		*/
	}
	
}

void bullet() { // пуля
	if (num_of_bullets > MAX_BULLETS) num_of_bullets = 0; //когда пуль в памяти более 100, записываем координаты новых в начало
	for (int i = 0; i < MAX_BULLETS; i++) {  //берет координаты каждой пули, добавляет к ним скорость и рисует их. 
		puli[i][0] += speed_of_bullet;
		glLineWidth(5);
		glBegin(GL_LINES);
		glColor3f(1, 0.4, 0); glVertex3f(puli[i][0], puli[i][1], 0);
		glColor3f(1, 1, 0); glVertex3f(puli[i][0] - 6, puli[i][1], 0);
		glEnd();
	}
}

void spaceship_move(unsigned char key, int x, int y) {  //перемещение корабля
	if ((key == 'w') || (key == 'W') || (key == '8')) {
		if ((difficulty == 0) && (choose > 1)) choose--;  //перемещает черный прямоугольник
		if (yCoord > -70) yCoord -= move;  // -= т.к. перевернута система координат (а это нужно для надписей)
	}
	if ((key == 's') || (key == 'S') || (key == '2')) {
		if ((difficulty == 0) && (choose < 3)) choose++; //перемещает черный прямоугольник
		if (yCoord < 90) yCoord += move;
	}
	if (((key == 'd') || (key == 'D') || (key == '6')) && (xCoord < 95)) xCoord += move;
	if (((key == 'a') || (key == 'A') || (key == '4')) && (xCoord > -75)) xCoord -= move;
	if ((key == ' ') || (key == '5')) {
		if (clock() - last_shooted_bullet > 150) { // проверяем, чтоб прошло какое-то время после последней пули
			last_shooted_bullet = clock();
			puli[num_of_bullets][0] = xCoord;
			puli[num_of_bullets][1] = yCoord;
			num_of_bullets++;
		}
	}
	if (key == 13) {
		difficulty = choose; // чем меньше значение, тем больше вероятность
		if (difficulty == 1) posibility_of_spawn_asteroids = 30;
		if (difficulty == 2) posibility_of_spawn_asteroids = 20;
		if (difficulty == 3) {
			posibility_of_spawn_asteroids = 10;
			speed_of_asteroids += 0.5;
		}
	}
	if (key == 27) {
		if (difficulty != -1) difficulty = -2;
	}
}

void menu() {
	char c1[40] = "Choose difficulty:";
	print_string(-48, -40, c1, 1, 1, 1);
	int dist; //перемещает черный прямоугольник в зависимости от выбора
	if (choose > 0) {
		if (choose == 1) dist = 0;
		if (choose == 2) dist = 15;
		if (choose == 3) dist = 30;
		glBegin(GL_POLYGON);
		glColor3f(0.3, 0.3, 0.4); glVertex3f(-50, -26 + dist, 1);
		glColor3f(0.3, 0.3, 0.4); glVertex3f(-50, -16 + dist, 1);
		glColor3f(0.3, 0.3, 0.4); glVertex3f(45, -16 + dist, 1);
		glColor3f(0.3, 0.3, 0.4); glVertex3f(45, -26 + dist, 1);
		glEnd();
	}

	char c2[10] = "- Easy";
	char c3[10] = "- Medium";
	char c4[10] = "- Hard";
	print_string(-48, -25, c2, 0, 1, 0);
	print_string(-48, -10, c3, 1, 1, 0);
	print_string(-48, 5, c4, 1, 0, 0);
}

void interface() {
	char str_score[9];
	int copy_score = score; // копия очков, с которой можно проводить разные операции
	for (int i = 0; i < 8; i++) {
		str_score[8 - i - 1] = copy_score % 10 + '0';
		copy_score = copy_score / 10;
	}
	str_score[8] = '\0';
	print_string(50, -90, str_score, 1, 1, 1);

	double heart_size = 0.6;
	if (lives >= 1) {
		int xHeart1 = 00;
		int yHeart1 = -88;
		glBegin(GL_POLYGON);
		glColor3f(1, 0, 0); glVertex3f(xHeart1, yHeart1 - 7 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart1 + 3 * heart_size, yHeart1 - 9 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart1 + 6 * heart_size, yHeart1 - 9 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart1 + 9 * heart_size, yHeart1 - 6 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart1 + 9 * heart_size, yHeart1 - 2 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart1 + 6 * heart_size, yHeart1 + 3 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart1, yHeart1 + 9 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart1 - 6 * heart_size, yHeart1 + 3 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart1 - 9 * heart_size, yHeart1 - 2 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart1 - 9 * heart_size, yHeart1 - 6 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart1 - 6 * heart_size, yHeart1 - 9 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart1 - 3 * heart_size, yHeart1 - 9 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart1 - 0 * heart_size, yHeart1 - 7 * heart_size, 0);
		glEnd();
	}
	if (lives >= 2) {
		int xHeart2 = 15;
		int yHeart2 = -88;
		glBegin(GL_POLYGON);
		glColor3f(1, 0, 0); glVertex3f(xHeart2, yHeart2 - 7 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart2 + 3 * heart_size, yHeart2 - 9 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart2 + 6 * heart_size, yHeart2 - 9 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart2 + 9 * heart_size, yHeart2 - 6 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart2 + 9 * heart_size, yHeart2 - 2 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart2 + 6 * heart_size, yHeart2 + 3 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart2, yHeart2 + 9 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart2 - 6 * heart_size, yHeart2 + 3 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart2 - 9 * heart_size, yHeart2 - 2 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart2 - 9 * heart_size, yHeart2 - 6 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart2 - 6 * heart_size, yHeart2 - 9 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart2 - 3 * heart_size, yHeart2 - 9 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart2 - 0 * heart_size, yHeart2 - 7 * heart_size, 0);
		glEnd();
	}
	if (lives >= 3) {
		int xHeart3 = 30;
		int yHeart3 = -88;
		glBegin(GL_POLYGON);
		glColor3f(1, 0, 0); glVertex3f(xHeart3, yHeart3 - 7 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart3 + 3 * heart_size, yHeart3 - 9 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart3 + 6 * heart_size, yHeart3 - 9 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart3 + 9 * heart_size, yHeart3 - 6 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart3 + 9 * heart_size, yHeart3 - 2 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart3 + 6 * heart_size, yHeart3 + 3 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart3, yHeart3 + 9 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart3 - 6 * heart_size, yHeart3 + 3 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart3 - 9 * heart_size, yHeart3 - 2 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart3 - 9 * heart_size, yHeart3 - 6 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart3 - 6 * heart_size, yHeart3 - 9 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart3 - 3 * heart_size, yHeart3 - 9 * heart_size, 0);
		glColor3f(1, 0, 0); glVertex3f(xHeart3 - 0 * heart_size, yHeart3 - 7 * heart_size, 0);
		glEnd();
	}

	if (time_x2_bonus > 0) {
		double xPos = -70;
		double yPos = -90;
		double size = 0.4;
		time_x2_bonus -= 5;

		glLineWidth(size * 32);
		glBegin(GL_LINES);
		glColor3f(1, 1, 0); glVertex3f(xPos + 20 * size, yPos + 0 * size, 0);
		glColor3f(1, 1, 0); glVertex3f(xPos + 20 * size + 0.01 * time_x2_bonus, yPos + 0 * size, 0);
		glEnd();

		glLineWidth(size * 8);
		glBegin(GL_LINES);
		glColor3f(1, 0, 0); glVertex3f(xPos - 9 * size, yPos - 9 * size, 0); // X начало
		glColor3f(1, 0, 0); glVertex3f(xPos - 3 * size, yPos + 9 * size, 0);

		glColor3f(1, 0, 0); glVertex3f(xPos - 3 * size, yPos - 9 * size, 0);
		glColor3f(1, 0, 0); glVertex3f(xPos - 9 * size, yPos + 9 * size, 0); // Х конец

		glColor3f(1, 0, 0); glVertex3f(xPos + 1 * size, yPos - 8 * size, 0);
		glColor3f(1, 0, 0); glVertex3f(xPos + 8 * size, yPos - 8 * size, 0);

		glColor3f(1, 0, 0); glVertex3f(xPos + 8 * size, yPos - 8 * size, 0);
		glColor3f(1, 0, 0); glVertex3f(xPos - 0 * size, yPos + 8 * size, 0);

		glColor3f(1, 0, 0); glVertex3f(xPos - 0 * size, yPos + 8 * size, 0);
		glColor3f(1, 0, 0); glVertex3f(xPos + 8 * size, yPos + 8 * size, 0);
		glEnd();

		glLineWidth(size * 4);
		glBegin(GL_LINES);
		glColor3f(1, 1, 0); glVertex3f(xPos - 13 * size, yPos + 12 * size, 0);
		glColor3f(1, 1, 0); glVertex3f(xPos - 13 * size, yPos - 12 * size, 0);

		glColor3f(1, 1, 0); glVertex3f(xPos - 12 * size, yPos - 13 * size, 0);
		glColor3f(1, 1, 0); glVertex3f(xPos + 12 * size, yPos - 13 * size, 0);

		glColor3f(1, 1, 0); glVertex3f(xPos + 13 * size, yPos - 12 * size, 0);
		glColor3f(1, 1, 0); glVertex3f(xPos + 13 * size, yPos + 12 * size, 0);

		glColor3f(1, 1, 0); glVertex3f(xPos + 12 * size, yPos + 13 * size, 0);
		glColor3f(1, 1, 0); glVertex3f(xPos - 12 * size, yPos + 13 * size, 0);
		glEnd();

		
	}
}

void pause() {
	print_string(-75, -30, "          PAUSE\n (press enter to continue)", 1, 1, 1);
}

void game_end_screen() {
	//print_string(-55, -30, "     EbATb Tbl\n Dolbayeb konechno...", 1, 0, 0);
	print_string(-30, -30, "GAME OVER", 1, 0, 0);
}

void check_hitted_asteroid_help(Object* asteroid, int j) {
	if (asteroid == NULL) return;

	if ((asteroid->yCoord - size_first_asteroid <= puli[j][1]) && (asteroid->yCoord + size_first_asteroid >= puli[j][1])) { //если пуля попала в диапазон ширины астероида
		if ((asteroid->xCoord - size_first_asteroid <= puli[j][0]) && (asteroid->xCoord >= puli[j][0])) { // и их координаты по х примерно равны
			puli[j][1] = 20000; // отправляем их обоих за карту
			//asteroid->yCoord = 1000;
			delete_node(asteroid);
			score++;
			if (time_x2_bonus > 0) score++;
			return;
		}
		else {
			check_hitted_asteroid_help(asteroid->pRight, j);
			check_hitted_asteroid_help(asteroid->pLeft, j);
		}
	}

	else if ((asteroid->yCoord - size_first_asteroid < puli[j][1])) { //если пуля попала в диапазон ширины астероида {
		check_hitted_asteroid_help(asteroid->pRight, j);
	}
	else {
		check_hitted_asteroid_help(asteroid->pLeft, j);
	}

}

void check_hitted_asteroid() {
	for (int j = 0; j < MAX_BULLETS; j++) {
		check_hitted_asteroid_help(asteroid_tree, j);
	}
}

/*
void check_hitted_asteroid() { // проверяет попала ли пуля в астероид. если да - отправляет его за карту
	for (int i = 0; i < MAX_ASTEROIDS; i++) {
		for (int j = 0; j < MAX_BULLETS; j++) {
			if ((asteroids[i][1] - size_first_asteroid <= puli[j][1]) && (asteroids[i][1] + size_first_asteroid >= puli[j][1])) { //если пуля попала в диапазон ширины астероида
				if ((asteroids[i][0] - size_first_asteroid <= puli[j][0]) && (asteroids[i][0] >= puli[j][0])) { // и их координаты по х примерно равны
					puli[j][1] = 20000; // отправляем их обоих за карту
					asteroids[i][1] = 10000; 
					score++;
					if (time_x2_bonus > 0) score++; 
				}
			}
		}
	}
}
*/


void check_hitted_spaceship(Object* asteroid) {
	if (asteroid == NULL) return;
	if ((asteroid->yCoord - size_first_asteroid - size_of_spaceship <= yCoord) &&
		(asteroid->yCoord + size_first_asteroid + size_of_spaceship >= yCoord)) {
		if ((asteroid->xCoord - size_of_spaceship <= xCoord) && (asteroid->xCoord + size_of_spaceship * 4 >= xCoord)) {
			if (clock() - last_lost_life > REGENIGATION_TIME) { // 2500 тиков - время форы перед новым снятием сердца
				lives--;
				last_lost_life = clock();
				if (lives == 0) {
					difficulty = -1; // если жизни кончились - проигрываем :)
					choose = 0; // чтоб нельзя было возродится нажав enter
					score = 0;
					lives = 3;
					for (int i = 0; i < MAX_BONUS; i++) bonuses[i][1] = 50000; // same situation
					for (int i = 0; i < MAX_STARS; i++) stars[i].yCoord = 30000; //в начале все звезды стоят по центру, т.к. в массиве нули. отрправляем их подальше
					for (int i = 0; i < MAX_BULLETS; i++) puli[i][1] = 20000; // same situation
					//for (int i = 0; i < MAX_ASTEROIDS; i++) asteroids[i][1] = 10000; // same situation
					asteroid_tree = NULL;
				}
				return;
			}
		}
		else {  // если на выбранном y не совпали х
			check_hitted_spaceship(asteroid->pRight); // вызываем функции с двух сторон, т.к. астероид врезающийся в корабль 
			check_hitted_spaceship(asteroid->pLeft);  // может быть как выше, так и ниже. Может быть на той же высоте, но это так же будет рассмотрено
		}
	}
	else if (asteroid->yCoord - size_first_asteroid - size_of_spaceship < yCoord) {
		check_hitted_spaceship(asteroid->pRight);
	}
	else {
		check_hitted_spaceship(asteroid->pLeft);
	}
}

/*
void check_hitted_spaceship() {
	Object* p = asteroid_tree;
	while (p != NULL) {
		if ((p->yCoord - size_first_asteroid - size_of_spaceship <= yCoord) &&
			(p->yCoord + size_first_asteroid + size_of_spaceship >= yCoord)) {
			if ((p->xCoord - size_of_spaceship <= xCoord) && (p->xCoord + size_of_spaceship * 4 >= xCoord)) {
				if (clock() - last_lost_life > REGENIGATION_TIME) { // 2500 тиков - время форы перед новым снятием сердца
					lives--;
					last_lost_life = clock();
					if (lives == 0) {
						difficulty = -1; // если жизни кончились - проигрываем :)
						choose = 0; // чтоб нельзя было возродится нажав enter
						score = 0;
						lives = 3;
						for (int i = 0; i < MAX_BONUS; i++) bonuses[i][1] = 50000; // same situation
						for (int i = 0; i < MAX_STARS; i++) stars[i].yCoord = 30000; //в начале все звезды стоят по центру, т.к. в массиве нули. отрправляем их подальше
						for (int i = 0; i < MAX_BULLETS; i++) puli[i][1] = 20000; // same situation
						//for (int i = 0; i < MAX_ASTEROIDS; i++) asteroids[i][1] = 10000; // same situation
					}
				}
			}
			else {  // если на выбранном y не совпали х

			}
		}
		else if (p->yCoord - size_first_asteroid - size_of_spaceship < yCoord) {
			p = p->pRight;
		}
		else {
			p = p->pLeft;
		}
	}
	*/

	/*
	for (int i = 0; i < MAX_ASTEROIDS; i++) {
		if ((asteroids[i][1] - size_first_asteroid - size_of_spaceship <= yCoord) &&
			(asteroids[i][1] + size_first_asteroid + size_of_spaceship >= yCoord)) {
			if ((asteroids[i][0] - size_of_spaceship <= xCoord) && (asteroids[i][0] + size_of_spaceship * 4 >= xCoord)) {
				if (clock() - last_lost_life > REGENIGATION_TIME) { // 2500 тиков - время форы перед новым снятием сердца
					lives--;
					last_lost_life = clock();
					if (lives == 0) {
						difficulty = -1; // если жизни кончились - проигрываем :)
						choose = 0; // чтоб нельзя было возродится нажав enter
						score = 0;
						lives = 3;
						for (int i = 0; i < MAX_BONUS; i++) bonuses[i][1] = 50000; // same situation
						for (int i = 0; i < MAX_STARS; i++) stars[i].yCoord = 30000; //в начале все звезды стоят по центру, т.к. в массиве нули. отрправляем их подальше
						for (int i = 0; i < MAX_BULLETS; i++) puli[i][1] = 20000; // same situation
						for (int i = 0; i < MAX_ASTEROIDS; i++) asteroids[i][1] = 10000; // same situation
					}
				}
			}
		}
	}
	*/


void check_given_bonus() {


	
	for (int i = 0; i < MAX_BONUS; i++) {
		if ((bonuses[i][1] - size_of_bonus - size_of_spaceship <= yCoord) &&
			(bonuses[i][1] + size_of_bonus + size_of_spaceship >= yCoord)) {
			if ((bonuses[i][0] - size_of_spaceship <= xCoord) && (bonuses[i][0] + size_of_spaceship * 4 >= xCoord)) {
				if (clock() - last_taken_bonus > CAN_TAKE_NEW_BONUS) {
					if (bonuses[i][3] == 1) { // доп жизни
						if (lives < 3) lives++;
						last_taken_bonus = clock();
						bonuses[i][1] = 50000;
					}
					if (bonuses[i][3] == 2) { // мультипликатор очков
						time_x2_bonus = 4000;  // время действия бонуса
						last_taken_bonus = clock();
						bonuses[i][1] = 50000;
					}
				}
			}
		}
	}
	
}

void creating_objects() {

	// пытаемся создать астероид
	if ((rand() % posibility_of_spawn_asteroids) == 9) {   //выбираем случайное время, при достижении которого генерируется астероид
		Object item;
		double y = ((rand() % 16) * 10) - 70; //выбирается случайное значение высоты для появившегося астероида. 70 и 16 (вместо 90 и 18) - немного сдвигаем вниз, чтоб не залезали на интерфейс
		item.xCoord = 100;  //начальная координата по х. спавним справа от экрана
		item.yCoord = y;
		item.speed = (speed_of_asteroids * (rand() % 10)) / 10 + 0.1;  // скорость. добавляем константу, чтоб те астероиды, у которых скорость выпала 0 тоже двигались.
		item.time_of_create = clock();
		asteroid_tree = add_to_tree(asteroid_tree, item);
		num_of_asteroids++;
	}

	if ((rand() % posibility_of_spawn_bonus) == 9) {   //выбираем случайное время, при достижении которого генерируется звезда. чем больше значение после %, тем ниже вероятность появления
		double y = ((rand() % 18) * 10) - 75; //выбирается случайное значение высоты для появившейся звезды. 75 (вместо 90) - немного сдвигаем вниз, чтоб не залезали на интерфейс
		bonuses[num_of_bonus][0] = 100;  //начальная координата по х. спавним справа от экрана
		bonuses[num_of_bonus][1] = y;
		bonuses[num_of_bonus][2] = speed_of_bonus;  // скорость. добавляем константу, чтоб те звезды, у которых скорость выпала 0 тоже двигались.
		bonuses[num_of_bonus][3] = rand() % 3;
		num_of_bonus++;
	}
}

void display() {
	glClear(GL_COLOR_BUFFER_BIT);
	if (difficulty > 0) { // на переднем плане рисуется то, что здесь стоит последним

		draw_stars();
		bullet();
		draw_asteroids(asteroid_tree);
		draw_bonuses();

		spaceship();

		creating_objects();
		check_given_bonus();
		check_hitted_spaceship(asteroid_tree);
		check_hitted_asteroid();

		interface();
	}
	else if (difficulty == 0) menu();
	else if (difficulty == -1) { // 4 - проиграл
		game_end_screen();

	}
	else if (difficulty = -2) {
		pause();
	}
	
	glutSwapBuffers();
}

void time_my(int num) {
	glutPostRedisplay();
	glutTimerFunc(16, time_my, 0);
}

int main(int argc, char** argv) {
	// ВАЖНО ОТПРАВЛЯТЬ ВСЕХ НА РАЗНУЮ ВЫСОТУ!
	for (int i = 0; i < MAX_BONUS; i++) bonuses[i][1] = 50000; // same situation
	for (int i = 0; i < MAX_STARS; i++) stars[i].yCoord = 30000; //в начале все звезды стоят по центру, т.к. в массиве нули. отрправляем их подальше
	for (int i = 0; i < MAX_BULLETS; i++) puli[i][1] = 20000; // same situation
	//for (int i = 0; i < MAX_ASTEROIDS; i++) asteroids[i][1] = 10000; // same situation

	last_shooted_bullet = clock();
	last_lost_life = - REGENIGATION_TIME; // чтоб не моргал в начале

	srand(time(NULL));

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowPosition(80, 80);
	glutInitWindowSize(600, 400);
	glutCreateWindow("Space Impact");
	glClearColor(0.08, 0.08, 0.13, 1);

	if (difficulty == 0) glScalef(0.01, -0.01, 0.1); //масштабируем текст заранее
	glutDisplayFunc(display);
	glutKeyboardFunc(spaceship_move);

	glutTimerFunc(0, time_my, 0);

	glutMainLoop();
}

//test changes
