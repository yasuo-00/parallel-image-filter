/*
Atividade 1 - Smooth - Programação Paralela e Distribuída
Prof Hélio Crestana
Alunos: Mario Luiz Gambim 744345 - Matheus Tomiyama 746040

o problema de suavização de pixels (smooth) numa imagem RGBA usando pthreads. Nos estudos de paralelização,
este é um exemplo de problema cuja divisão dos dados pode envolver o acesso a dados na vizinhança. 

Estratégia de paralelização: Particionamento. Dividiu-se a matriz RGBA da imagem de entrada pelo número de threads
							para a realização da técnica de smoothing.
							Esperou-se todas as threads terminarem retornou-se a imagem resultante

Speedup: Foram realizadas 10 execuções paralelas( 4 threads) e 10 execuções sequenciais (1 thread), os resultados foram os seguintes:
		Execução sequencial:
				Tempo de execução:
					Max: 0.037749 segundos
					Min: 0.033328 segundos
					Média: 0.0354359 segundos
		
		Execuçao paralela:
				Tempo de execução:
					Max: 0.013938 segundos
					Min: 0.013253 segundos
					Média: 0.0134891 segundos
		
		Speedup médio: 2.627

*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#define WIDTH 512
#define HEIGHT 512
#define NTHREADS 8

//matriz de entrada recebe 2 colunas e linhas adicionais para tratamento da borda
int mr[WIDTH + 2][HEIGHT + 2];
int mg[WIDTH + 2][HEIGHT + 2];
int mb[WIDTH + 2][HEIGHT + 2];
int ma[WIDTH + 2][HEIGHT + 2];

//matriz resultante possui tamanho passado
int mr2[WIDTH][HEIGHT];
int mg2[WIDTH][HEIGHT];
int mb2[WIDTH][HEIGHT];
int ma2[WIDTH][HEIGHT];

//struct utilizado para passar parâmetros na função pthread_create
struct range_matriz
{
	int linhaInicial;
	int linhaFinal;
};

void filtro(int linha, int coluna)
{
	int stencilR[3][3], stencilG[3][3], stencilB[3][3], stencilA[3][3]; //stencil do componente correspondente
	int sumR = 0, sumG = 0, sumB = 0, sumA = 0;							//soma dos componente correspondente
	int avgR, avgG, avgB, avgA;											// média do componente correspondente

	//iStencil,jStencil para percorrer a matriz stencil
	for (int iStencil = linha - 1; iStencil < linha + 2; iStencil++)
	{
		for (int jStencil = coluna - 1; jStencil < coluna + 2; jStencil++)
		{
			//a soma dentro dos indices é parar garantir que o indice esteja entre 0 e 2
			stencilR[iStencil - (linha - 1)][jStencil - (coluna - 1)] = mr[iStencil][jStencil];
			stencilG[iStencil - (linha - 1)][jStencil - (coluna - 1)] = mg[iStencil][jStencil];
			stencilB[iStencil - (linha - 1)][jStencil - (coluna - 1)] = mb[iStencil][jStencil];
			stencilA[iStencil - (linha - 1)][jStencil - (coluna - 1)] = ma[iStencil][jStencil];
			//faz a soma de todos os elementos de  cada componente
			sumR += stencilR[iStencil - (linha - 1)][jStencil - (coluna - 1)];
			sumG += stencilG[iStencil - (linha - 1)][jStencil - (coluna - 1)];
			sumB += stencilB[iStencil - (linha - 1)][jStencil - (coluna - 1)];
			sumA += stencilA[iStencil - (linha - 1)][jStencil - (coluna - 1)];
		}
	}
	//cálculo das médias
	avgR = sumR / 9;
	avgG = sumG / 9;
	avgB = sumB / 9;
	avgA = sumA / 9;

	//escrita na matriz resultante
	mr2[linha - 1][coluna - 1] = avgR;
	mg2[linha - 1][coluna - 1] = avgG;
	mb2[linha - 1][coluna - 1] = avgB;
	ma2[linha - 1][coluna - 1] = avgA;
}

void *smooth(void *pos)
{
	//desestrutura a entrada para pegar as linhas da thread
	int linhaInicial = ((struct range_matriz *)pos)->linhaInicial;
	int linhaFinal = ((struct range_matriz *)pos)->linhaFinal;
	//iMatriz,jMatriz para percorrer a matriz
	//percorre da linhaInicial+1, jMatriz=1 até iMatriz<=linhaFinalm jMatriz<=WIDTH por causa das 2 linhas e colunas adicionais
	for (int iMatriz = linhaInicial + 1; iMatriz <= linhaFinal; iMatriz++)
	{
		for (int jMatriz = 1; jMatriz <= WIDTH; jMatriz++)
		{
			//chama funcao de stencil para o elemento
			filtro(iMatriz, jMatriz);
		}
	}
}

int main(int argc, char **argv)
{
	int i, j;
	int fdi, fdo;
	int nlin = 0;
	int ncol = 0;
	char name[128];
	int status;
	pthread_t threads[NTHREADS];
	// variáveis para medida do tempo
	struct timeval inic, fim;
	struct rusage r1, r2;

	if (argc < 2)
	{
		printf("Uso: %s nome_arquivo\n", argv[0]);
		exit(0);
	}
	if ((fdi = open(argv[1], O_RDONLY)) == -1)
	{
		printf("Erro na abertura do arquivo %s\n", argv[1]);
		exit(0);
	}

	// lê arquivo de imagem
	// formato:
	//		ncolunas (2bytes) - largura
	//		nlinhas (2bytes) - altura
	// dados: rgba (4bytes)

	/*
 * Supondo imagem 512x512, do exemplo, não mantemos tamanho no arquivo
 */
	/* 	if(read(fdi,&ncol,2)==-1) {
		printf("Erro na leitura do arquivo\n");		
		exit(0);
	}
	if(read(fdi,&nlin,2)==-1) {
		printf("Erro na leitura do arquivo\n");		
		exit(0);
	}
	printf("Tamanho da imagem: %d x %d\n",ncol,nlin); */

	nlin = ncol = 512;

	// zerar as matrizes (4 bytes, mas usaremos 1 por pixel)
	// void *memset(void *s, int c, size_t n);
	//matriz de entrada recebe 2 colunas e linhas adicionais para tratamento da borda
	memset(mr, 0, (nlin + 2) * (ncol + 2) * sizeof(int));
	memset(mg, 0, (nlin + 2) * (ncol + 2) * sizeof(int));
	memset(mb, 0, (nlin + 2) * (ncol + 2) * sizeof(int));
	memset(ma, 0, (nlin + 2) * (ncol + 2) * sizeof(int));

	memset(mr2, 0, (nlin) * (ncol) * sizeof(int));
	memset(mg2, 0, (nlin) * (ncol) * sizeof(int));
	memset(mb2, 0, (nlin) * (ncol) * sizeof(int));
	memset(ma2, 0, (nlin) * (ncol) * sizeof(int));

	// (ao menos) 2 abordagens:
	// - ler pixels byte a byte, colocando-os em matrizes separadas
	//	- ler pixels (32bits) e depois usar máscaras e rotações de bits para o processamento.

	// ordem de leitura dos bytes (componentes do pixel) depende se o formato
	// é little ou big endian
	// Assumindo little endian
	//ler a imagem do arquivo e colocar na matriz de entrada nas linhas 1 até nlin+1, nas colunas 1 até ncol+1
	// (por causa da borda adicionada)
	for (i = 1; i < nlin + 1; i++)
	{
		for (j = 1; j < ncol + 1; j++)
		{
			read(fdi, &mr[i][j], 1);
			read(fdi, &mg[i][j], 1);
			read(fdi, &mb[i][j], 1);
			read(fdi, &ma[i][j], 1);
		}
	}
	close(fdi);

	//repetir valores das linhas vizinhas para as linhas adicionadas
	for (j = 1; j < ncol + 1; j++)
	{
		mr[0][j] = mr[1][j];
		mr[nlin + 1][j] = mr[nlin][j];
		mg[0][j] = mg[1][j];
		mg[nlin + 1][j] = mg[nlin][j];
		mb[0][j] = mb[1][j];
		mb[nlin + 1][j] = mb[nlin][j];
		ma[0][j] = ma[1][j];
		ma[nlin + 1][j] = ma[nlin][j];
	}

	//repetir valores das colunas vizinhas para as colunas adicionadas
	for (i = 0; i < nlin + 3; i++)
	{
		mr[i][0] = mr[i][1];
		mr[i][nlin + 1] = mr[i][nlin];
		mg[i][0] = mg[i][1];
		mg[i][nlin + 1] = mg[i][nlin];
		mb[i][0] = mb[i][1];
		mb[i][nlin + 1] = mb[i][nlin];
		ma[i][0] = ma[i][1];
		ma[i][nlin + 1] = ma[i][nlin];
	}

	//pega o tempo antes da execução das threads
	gettimeofday(&inic, 0);

	//inicia um vetor de uma estrutura que contem a linha inicial e final que uma thread deve operar
	struct range_matriz *thread_range = (struct range_matriz *)malloc(sizeof(struct range_matriz) * NTHREADS);

	// cria uma thread para cada elemento da matriz desconsiderando os elementos da borda
	for (int i = 0; i < NTHREADS; i++)
	{
		//pega as linhas através do num_linhas/num_threads
		(thread_range + i)->linhaInicial = (i * ((HEIGHT) / NTHREADS));
		(thread_range + i)->linhaFinal = (i + 1) * ((HEIGHT) / NTHREADS);
		//criação das threads
		status = pthread_create(&threads[i], NULL, smooth, (void *)(thread_range + i));
		if (status)
		{
			//strerror_r(status,err_msg,LEN);
			printf("Falha da criacao da thread");
			exit(EXIT_FAILURE);
		}
	}
	// espera threads retornarem
	for (int t = 0; t < NTHREADS; t++)
	{
		//printf("%d\n",t);
		status = pthread_join(threads[t], NULL);
		if (status)
		{
			//strerror_r(status,err_msg,LEN);
			printf("Erro em pthread_join");
			exit(EXIT_FAILURE);
		}
	}

	//pega o tempo depois da execução das threads
	gettimeofday(&fim, 0);

	//imprime o tempo de execução (tempo_inicial-tempo_final)
	printf("\nTempo de execução: %f segundos\n",
		   (fim.tv_sec + fim.tv_usec / 1000000.) - (inic.tv_sec + inic.tv_usec / 1000000.));

	// gravar imagem resultante
	sprintf(name, "%s.new", argv[1]);
	fdo = open(name, O_WRONLY | O_CREAT);

	/*
	write(fdo,&ncol,2);
	write(fdo,&nlin,2);
*/

	for (i = 0; i < nlin; i++)
	{
		for (j = 0; j < ncol; j++)
		{
			write(fdo, &mr2[i][j], 1);
			write(fdo, &mg2[i][j], 1);
			write(fdo, &mb2[i][j], 1);
			write(fdo, &ma2[i][j], 1);
		}
	}
	close(fdo);

	return 0;
}
