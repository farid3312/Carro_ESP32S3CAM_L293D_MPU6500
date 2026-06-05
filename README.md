# CarroESP32S3CAMDeteccionDeCaidas
Carro con pocos sensores con conexion por web socket,Hace uso de esp32s3 con un modulo mpu6500 y 4 motores mas 2 baterias 18650, es una implementacion paso a paso, verificando cada uno de los componentes.

para la implementacion del carro controlado por pagina web, se necesita una buena conexion wifi.

Para la implementacion paso a paso se recomienda seguir la carga de los siguientes codigos:
1- iniciar por i2CTEST.INO este codigo sirve para probar si las conexiones al MPU 6500 funcionan correctamente y la esp32s3 detecta al modulo correctamente.
2-GY6500PLOTTER.ino  este usa la libreria basica de wire.h para tomar los valores en crudo y mostrarlos por el Plotter serial, con la finalidad de entender las varibales que mide el modulo.
3-GY6500_BASICO.ino este hace uso de la libreria MPU6500_WE para hacer funcionar el modulo de manera que se pueda ver en el puerto serial (tiene la finalidad de apreciar en mas detalle los valores de las variables)
4-GY-6500PLOTTERLIBRERIA.INO  es muy parecido al plotter del punto 2, pero en este caso haciendo uso de la libreria del punto 3, obteniendo un codigo mucho mas corto.
6- GY6500_MEDICION_VARIABLES.ino en este caso ya no se toman las variables en crudo, sino que se las adapata para entender los principios de pitch,roll,yaw  y medir la temperatura del modulo.

7- PruebaMotores.ino este es un codigo sencillo para probar las conexiones de los motores e identificar que pines se asignan para el correcto movimiento del carro.
8- esp32s3cam.ino este es un codigo para probar la camara a travez de web socket.
9- Caro_Integracion_Basico.ino este es un codigo que recoge los codigos de los sensores de manera basica para probarlos en integracion, permite el movimiento del carro y ver el carro a travez de la conexion por direccion IP.
10- Carro_Cam_GY_motores.ino este es una mejora del punto 7, permitiendo mas cambios por el apartado de la camará, verificacion de conexion y tomar los fps.
