#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

//5 seg = 1 pulso
//12 pulsos(1min) = 60seg/5seg
#define HOUR_READINGS 720 //720 = 1h
#define DAY_READINGS 17280 //17280 = 1 dia


void write_header(FILE *file) {
	//escreve o "Header" na tela e nos arq. de backup
	fprintf(file, "Data		Hora       Temp_atual	Temp_med	Temp_min	Temp_max      Umi_atual     Umi_med     Umi_min     Umi_max\n");
	fflush(file);
}

int main(void)
{
	int hour = 1;
	int day  = 1;
	
	//variaveis para descobrir variação min/max
	double tempC_min = 100000;
	double tempC_max = -100;
	double humi_min = 100000;
	double humi_max = -100;
    
	//variaveis para fazer a média constante
	double tempSumC_day = 0;
	double humiditySum_day = 0;
	double tempSumC_hour = 0;
	double humiditySum_hour = 0;
	double totalTempSum = 0;
	double totalTempHum = 0;
	double totalReadingCount=0;
    
	//cria a matriz de temperaturas HORA
	double tempReadingsC_hour[HOUR_READINGS];
	double humidityReadings_hour[HOUR_READINGS];
	int readingCount_hour = 0; //contador de leituras
	
	//cria a matriz de temperaturas DIA
	double tempReadingsC_day[DAY_READINGS];
	double humidityReadings_day[DAY_READINGS];
	int readingCount_day = 0; //contador de leituras
	
	//pegar tempo
	struct tm *localtime(const time_t *timer);
	
	
	
	// Create I2C bus (barramento)
	int file;
	char* bus = "/dev/i2c-1";
	if ((file = open(bus, O_RDWR)) < 0){
		printf("Failed to open the bus. \n");
		exit(1);
	}
	
	// Get I2C device, SHT31 I2C address is 0x44(68)
	ioctl(file, I2C_SLAVE, 0x44);

	
	//BACKUP SEGUNDOS - HORA - DIA, abrindo arq
	FILE *secBackup = fopen("/home/tecnico/agsolve/temp_umi_sensor/sec_backup.txt", "a"); //a = gravação de dados no fim do arq.
	FILE *hourBackup = fopen("/home/tecnico/agsolve/temp_umi_sensor/hour_backup.txt", "a");//w = gravação de dados destruindo arq anterior.
	FILE *dayBackup = fopen("/home/tecnico/agsolve/temp_umi_sensor/day_backup.txt", "a");
	if (dayBackup == NULL ||hourBackup == NULL ||secBackup == NULL) {
        printf("Failed to open the Backup File.\n");
        close(file);
        return 1;
	}
	
	//escreve na tela 1x
	printf("Data		Hora       Temp_atual	Temp_med	Temp_min	Temp_max      Umi_atual     Umi_med     Umi_min     Umi_max\n");
	
	//verifica se arquivo está vazio, se vazio = escreve header
	if (secBackup == NULL){
	    write_header(secBackup);
	}
	if (hourBackup == NULL) {
	    write_header(hourBackup);
	}
	if (dayBackup == NULL){
	    write_header(dayBackup);
	}
	
    
	while (1) { // "Sempre em loop"
		// Send high repeatability measurement command
		// Command msb, command lsb(0x2C, 0x06)
		char config[2] = { 0 };
		config[0] = 0x2C;
		config[1] = 0x06;
		write(file, config, 2);
		//sleep(1);

		// Read 6 bytes of data
		// temp msb, temp lsb, temp CRC, humidity msb, humidity lsb, humidity CRC
		char data[6] = { 0 };
		if (read(file, data, 6) != 6){
			printf("Error : Input/output Error \n");
			continue;
		}
		
		//Define o tempo = Dia + Hora
		time_t t = time(NULL);
		struct tm* tm_info = localtime(&t);
		char date[11];
		char time [9];
		strftime(date, sizeof(date), "%d/%m/%Y", tm_info);
		strftime(time, sizeof(time), "%H:%M:%S", tm_info);

		// Convert the data
		double cTemp = (((data[0] * 256) + data[1]) * 175.0) / 65535.0 - 45.0;
		double humidity = (((data[3] * 256) + data[4])) * 100.0 / 65535.0;
		
        
		//Descobrir maior / menor numero já registrado
		if(cTemp > tempC_max){
			tempC_max = cTemp;
		} 
		if (cTemp < tempC_min){
			tempC_min = cTemp;
		}
		
		if(humidity > humi_max){
			humi_max = humidity;
		}
		if(humidity < humi_min){
			humi_min = humidity;
		}
		
		
		//Somador para calcular a média constante e contador de numeros para divisão
		totalTempSum += cTemp;
		totalTempHum += humidity;
		totalReadingCount++;
		
		//calcular média constante
		double average_temp = (totalReadingCount > 0) ? (totalTempSum / totalReadingCount) : 0;        
		double average_humi = (totalReadingCount > 0) ? (totalTempHum / totalReadingCount) : 0;
		
		
		printf("%s	%s    %.2f °C	 %.2f °C	%.2f °C	 %.2f °C     %.2f RH      %.2f RH    %.2f RH    %.2f RH   \n", 
		date, time, cTemp, average_temp, tempC_min, tempC_max,humidity, average_humi, humi_min, humi_max);
		//armazena no backup segundos
		fprintf(secBackup, "%s	%s    %.2f °C	 %.2f °C	%.2f °C	 %.2f °C     %.2f RH      %.2f RH    %.2f RH    %.2f RH   \n", 
		date, time, cTemp, average_temp, tempC_min, tempC_max,humidity, average_humi, humi_min, humi_max);
		fflush(secBackup); 
		
		//armazena as leituras na matriz HORAS - DIA
		//se contador for menor que pulsos, coloque a temperatura na i[x]
		if (readingCount_hour < HOUR_READINGS) {
			tempReadingsC_hour[readingCount_hour] = cTemp;
			humidityReadings_hour[readingCount_hour] = humidity;
			readingCount_hour++;  //incrementa +1 a cada leitura(logo na prox leitura será i[2])
		}
		if (readingCount_day < DAY_READINGS) {
			tempReadingsC_day[readingCount_day] = cTemp;
			humidityReadings_day[readingCount_day] = humidity;
			readingCount_day++;
		}
       

		//a cada 1 hora, calcula e exibe as medias
		if (readingCount_hour == HOUR_READINGS) {
			
			//acumulador 
			for (int i = 0; i < HOUR_READINGS; i++) {
				tempSumC_hour += tempReadingsC_hour[i];
				humiditySum_hour += humidityReadings_hour[i];
			}
			
		double average_temp_hour = tempSumC_hour/HOUR_READINGS;
		double average_humi_hour = humiditySum_hour/HOUR_READINGS;

		//armazena no backup hora
		fprintf(hourBackup, "%s	%s    %.2f °C	 %.2f °C	%.2f °C	 %.2f °C     %.2f RH      %.2f RH    %.2f RH    %.2f RH   \n", 
		date, time, cTemp, average_temp_hour, tempC_min, tempC_max,humidity, average_humi_hour, humi_min, humi_max);
		fflush(hourBackup);

			
			//reinicia o cont
			readingCount_hour = 0;
			hour ++;

		}
		if (readingCount_day == DAY_READINGS) {//a cada 1 dia exibe as medias

			//acumulador 
			for (int i = 0; i < DAY_READINGS; i++) {
				tempSumC_day += tempReadingsC_day[i];
				humiditySum_day += humidityReadings_day[i]; //incrementa 
			}

			//calcula media
			double average_temp_day = tempSumC_day / DAY_READINGS;
			double average_humi_day = humiditySum_day / DAY_READINGS;
			
			//armazena no backup dia
			fprintf(dayBackup, "%s	%s    %.2f °C	 %.2f °C	%.2f °C	 %.2f °C     %.2f RH      %.2f RH    %.2f RH    %.2f RH   \n", 
			date, time, cTemp, average_temp_day, tempC_min, tempC_max,humidity, average_humi_day, humi_min, humi_max);
			fflush(dayBackup);

			//reinicia o cont
			readingCount_day = 0;
			day +=1;
		}
		
		//espera 5 seg p armazenar prox leitura
		sleep(5);
	}
	   
	fclose(hourBackup);
	fclose(dayBackup);
	fclose(secBackup);
	close(file);
	return 0;
	

	
	
}

