# Counter

Programa en C++ para contar la frecuencia de palabras en un archivo de texto, usando **OpenMP** para paralelizar partes del procesamiento.

## ¿Qué hace?

- Lee un archivo de texto.
- Cuenta cuántas veces aparece cada palabra.
- Usa OpenMP para mejorar el rendimiento en procesamiento paralelo.
- Genera un archivo de salida con el resultado de frecuencias.

## Compilación

```bash
g++ -02 -fopenmp counter.cpp -o app
```

## Ejecución

```bash
./app
```

Opcionalmente, puedes definir el número de hilos:

```bash
OMP_NUM_THREADS=4 ./Counter/counter
```

> Si tu programa recibe parámetros (archivo de entrada/salida), ejecútalo con los argumentos implementados en `counter.cpp`.