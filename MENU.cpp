#include <Arduino.h>
#include "MENU.h"

Menu::Menu(const STRUCT_MENU *menu, byte buffer_size)
{
	 current_menu_index=0;
	 menu_element_text = new char[buffer_size];
	_menu=menu;
}

void Menu::exec()
{
  void (*pf)() = pgm_read_word_near(&_menu[current_menu_index].func);
  if(pf!=0)
  {
    pf();
  }
  else
  {
    child();  
  }
  
}

void Menu::next()
{
  byte new_menu_index = pgm_read_word_near(&_menu[current_menu_index].next); 
  if(new_menu_index!=0)
  {
    current_menu_index=new_menu_index;
    print();
  }
}

void Menu::prev()
{
  byte new_menu_index = pgm_read_word_near(&_menu[current_menu_index].prev); 
  if(new_menu_index!=0)
  {
    current_menu_index=new_menu_index;
    print();
  }
}

void Menu::parent()
{
  byte new_menu_index = pgm_read_word_near(&_menu[current_menu_index].parent); 
  if(new_menu_index!=0)
  {
    current_menu_index=new_menu_index;
    print();
  }
}

void Menu::child()
{
  byte new_menu_index = pgm_read_word_near(&_menu[current_menu_index].child); 
  if(new_menu_index!=0)
  {
    current_menu_index=new_menu_index;
    print();
  }
}

void Menu::print()
{
  strcpy_P(menu_element_text, (char*)pgm_read_word(&_menu[current_menu_index].text));
  #ifdef SHOW_CHILDRENS_ICON
    Serial.print(menu_element_text);
    Serial.write(' ');
    byte childrens_exist = pgm_read_word_near(&_menu[current_menu_index].child);
    if(childrens_exist>0) 
    {
      Serial.println("\\/");  
    }
    else
    {
    Serial.write('\n');
    }
  #else
    Serial.println(menu_element_text);
  #endif
}

void Menu::set_curent_menu(byte index)
{
	current_menu_index = index;
}

byte Menu::get_curent_menu(){
	return current_menu_index; 
}

