#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define HOUR_READINGS 720 
#define DAY_READINGS 17280 


int sec_is_empty() {
	FILE *secBackup = fopen("/home/tecnico/agsolve/temp_umi_sensor/sec_backup.txt", "a"); 
    fseek(secBackup, 0, SEEK_END);
    long size = ftell(secBackup);
    fclose(secBackup);
    return size == 0; // Retorna 1 se vazio, 0 se não
}

int hour_is_empty() {
	FILE *hourBackup = fopen("/home/tecnico/agsolve/temp_umi_sensor/hour_backup.txt", "a"); 
    fseek(hourBackup, 0, SEEK_END);
    long size = ftell(hourBackup);
    fclose(hourBackup);
    return size == 0; // Retorna 1 se vazio, 0 se não
}

int day_is_empty() {
	FILE *dayBackup = fopen("/home/tecnico/agsolve/temp_umi_sensor/day_backup.txt", "a"); 
    fseek(dayBackup, 0, SEEK_END);
    long size = ftell(dayBackup);
    fclose(dayBackup);
    return size == 0; // Retorna 1 se vazio, 0 se não
}


void write_header(FILE *file) {
	fprintf(file, "Data		Hora       Temp_atual	Temp_med	Temp_min	Temp_max      Umi_atual     Umi_med     Umi_min     Umi_max\n");
	fflush(file);
}

int main(void)
{
	//variaveis temp/hum min/max
	double tempC_min = 100000; //começa c valor alto p conseguir o menor
	double tempC_max = -100;  //começa c valor baixo p conseguir o maior
	double humi_min = 100000;
	double humi_max = -100;
	//  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  
	double tempC_min_hour = 100000; //começa c valor alto p conseguir o menor
	double tempC_max_hour = -100;  //começa c valor baixo p conseguir o maior
	double humi_min_hour = 100000;
	double humi_max_hour = -100;
    
	//acumuladores -DIA
	double tempSumC_day = 0;
	double humiditySum_day = 0;
	//acumuladores -HORA
	double tempSumC_hour = 0;
	double humiditySum_hour = 0;
	//acumuladores p média constante
	double totalTempSum = 0;
	double totalTempHum = 0;
	// total de leituras - DIA - HORA - TOTAL
	double totalReadingCount = 0;
	double readingCount_hour = 0; 
	double readingCount_day = 0; 
	double readingCount_delete_file = 0; 
	
	struct tm *localtime(const time_t *timer);
	
	
	
	int file;
	char* bus = "/dev/i2c-1";
	if ((file = open(bus, O_RDWR)) < 0){
		printf("Failed to open the bus. \n");
		exit(1);
	}
	
	ioctl(file, I2C_SLAVE, 0x44);

	FILE *secBackup = fopen("/home/tecnico/agsolve/temp_umi_sensor/sec_backup.txt", "a"); 
	FILE *hourBackup = fopen("/home/tecnico/agsolve/temp_umi_sensor/hour_backup.txt", "a");
	FILE *dayBackup = fopen("/home/tecnico/agsolve/temp_umi_sensor/day_backup.txt", "a");
	
	if (dayBackup == NULL ||hourBackup == NULL ||secBackup == NULL) {
        printf("Failed to open the Backup File.\n");
        close(file);
        return 1;
	}
	
	printf("Data		Hora       Temp_atual	Temp_med	Temp_min	Temp_max      Umi_atual     Umi_med     Umi_min     Umi_max\n");

	if (sec_is_empty() == 1){
	    write_header(secBackup);
	}
	if (hour_is_empty() == 1){
	    write_header(hourBackup);
	}
	if (day_is_empty () == 1){
	     write_header(dayBackup);
	}
	
    
	while (1) { 

		char config[2] = { 0 };
		config[0] = 0x2C;
		config[1] = 0x06;
		write(file, config, 2);

		char data[6] = { 0 };
		if (read(file, data, 6) != 6){
			printf("Error : Input/output Error \n");
			continue;
		}
		
		time_t t = time(NULL);
		struct tm* tm_info = localtime(&t);
		char date[11];
		char time [9];
		strftime(date, sizeof(date), "%d/%m/%Y", tm_info);
		strftime(time, sizeof(time), "%H:%M:%S", tm_info);

		double cTemp = (((data[0] * 256) + data[1]) * 175.0) / 65535.0 - 45.0;
		double humidity = (((data[3] * 256) + data[4])) * 100.0 / 65535.0;
		
        //lógica maior/menor temp/umi
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
		//  //  //  //  //  //  //  //  //  //  //  //  //  //  //  //  
		if(cTemp > tempC_max_hour){
			tempC_max_hour = cTemp;
		} 
		if (cTemp < tempC_min_hour){
			tempC_min_hour = cTemp;
		}
		
		if(humidity > humi_max_hour){
			humi_max_hour = humidity;
		}
		if(humidity < humi_min_hour){
			humi_min_hour = humidity;
		}
		
		//acumulador p calcular a media
		totalTempSum  += cTemp;
		tempSumC_hour += cTemp;
		tempSumC_day  += cTemp;
		totalTempHum     += humidity;
		humiditySum_hour += humidity;
		humiditySum_day  += humidity;
		
		totalReadingCount++;
		
		double average_temp = (totalReadingCount > 0) ? (totalTempSum / totalReadingCount) : 0;        
		double average_humi = (totalReadingCount > 0) ? (totalTempHum / totalReadingCount) : 0;
		
		
		printf("%s	%s    %.2f °C	 %.2f °C	%.2f °C	 %.2f °C     %.2f RH      %.2f RH    %.2f RH    %.2f RH   \n", 
		date, time, cTemp, average_temp, tempC_min, tempC_max,humidity, average_humi, humi_min, humi_max);

		fprintf(secBackup, "%s	%s    %.2f °C	 %.2f °C	%.2f °C	 %.2f °C     %.2f RH      %.2f RH    %.2f RH    %.2f RH   \n", 
		date, time, cTemp, average_temp, tempC_min, tempC_max,humidity, average_humi, humi_min, humi_max);
		fflush(secBackup); 
		
		if (readingCount_hour < HOUR_READINGS) {
			readingCount_hour++;  //incrementa +1 a cada leitura(logo na prox leitura será i[2])
		}
		if (readingCount_day < DAY_READINGS) {
			readingCount_day++;
		}
		if ( readingCount_delete_file < 34560){
			readingCount_delete_file++;
		}
       
		//a cada 1h
		if (readingCount_hour == HOUR_READINGS) {
			
			double average_temp_hour = (readingCount_hour > 0) ? (tempSumC_hour/readingCount_hour) : 0; 
			double average_humi_hour = (readingCount_hour > 0) ? (humiditySum_hour/readingCount_hour) : 0; 

			fprintf(hourBackup, "%s	%s    %.2f °C	 %.2f °C	%.2f °C	 %.2f °C     %.2f RH      %.2f RH    %.2f RH    %.2f RH   \n", 
			date, time, cTemp, average_temp_hour, tempC_min_hour, tempC_max_hour,humidity, average_humi_hour, humi_min_hour, humi_max_hour);
			fflush(hourBackup);

			//zera variáveis
			readingCount_hour = 0;
			humiditySum_hour = 0;
			tempSumC_hour = 0;
			
			//zera para obter corretamente a cada 1h	
			tempC_min_hour = 100000; //começa c valor alto p conseguir o menor
			tempC_max_hour = -100;  //começa c valor baixo p conseguir o maior
			humi_min_hour = 100000;
			humi_max_hour = -100;
		}
		
		//a cada 1d
		if (readingCount_day == DAY_READINGS) {
 
			double average_temp_day = (readingCount_day > 0) ? (tempSumC_day/readingCount_day) : 0; 
			double average_humi_day = (readingCount_day > 0) ? (humiditySum_day/readingCount_day) : 0; 

			
			fprintf(dayBackup, "%s	%s    %.2f °C	 %.2f °C	%.2f °C	 %.2f °C     %.2f RH      %.2f RH    %.2f RH    %.2f RH   \n", 
			date, time, cTemp, average_temp_day, tempC_min, tempC_max,humidity, average_humi_day, humi_min, humi_max);
			fflush(dayBackup);
			
			//zera variáveis
			readingCount_day = 0;
			humiditySum_day = 0;
			tempSumC_day = 0;
			
			//zera para obter corretamente a cada 24h
			tempC_min = 100000;
			tempC_max = -100;
			humi_min = 100000;
			humi_max = -100;

		}
		
		if (readingCount_day == 34560) {
			
			//Deleta arq. segundos a cada 24h
			//remove("/home/tecnico/agsolve/temp_umi_sensor/sec_backup.txt");
			readingCount_delete_file = 0;
		}


		sleep(5);
	}
	   
	fclose(hourBackup);
	fclose(dayBackup);
	fclose(secBackup);
	close(file);
	return 0;
	

	
	
}

