/*
  MENU.h - Библиотека для обхода пунктов древовоидного меню
  Разработано XanderEVG. Тюмень, 11.10.2017
*/

#ifndef MENU_H
#define MENU_H
 
#include <Arduino.h>
#include <avr/pgmspace.h>

#define SHOW_CHILDRENS_ICON


struct STRUCT_MENU {
  const char *text;
  byte next;
  byte prev;
  byte parent;
  byte child;
  void *func;
};

 
class Menu
{
  public:
    Menu(const STRUCT_MENU *menu, byte buffer_size);
    void exec();
  	void next();
  	void prev();
  	void parent();
  	void child();
  	void print();
  	void set_curent_menu(byte index);
  	byte get_curent_menu();
    
  private:
    const STRUCT_MENU *_menu;
  	char *menu_element_text;
  	byte current_menu_index;
};
 
#endif
