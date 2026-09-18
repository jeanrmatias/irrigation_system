#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include "DHT.h"

#define DHTPIN 4     
#define DHTTYPE DHT11 

extern const int pinoSensor;   
extern float umidade;
extern float temperatura;
extern int valorLeitura;

void inicializarSensores();
void lerSensores();

#endif