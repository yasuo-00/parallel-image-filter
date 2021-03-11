/*
Atividade 1 - Smooth - Programação Paralela e Distribuída
Prof Hélio Crestana
Alunos: Mario Luiz Gambim 744345 - Matheus Tomiyama 746040

o problema de suavização de pixels (smooth) numa imagem RGBA usando pthreads. Nos estudos de paralelização,
este é um exemplo de problema cuja divisão dos dados pode envolver o acesso a dados na vizinhança. 
Anexe aqui o código em C com comentários sobre a estratégia de paralelização, potencial speedup e as sincronizações necessárias.

Estratégia de paralelização:
Speedup:

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

#define WIDTH  512
#define HEIGHT 512
#define NTHREADS 8

//matriz original recebe 2 colunas e linhas adicionais para tratamento da borda
int mr [WIDTH+2][HEIGHT+2];
int mg [WIDTH+2][HEIGHT+2];
int mb [WIDTH+2][HEIGHT+2];
int ma [WIDTH+2][HEIGHT+2];
int mr2 [WIDTH][HEIGHT];
int mg2 [WIDTH][HEIGHT];
int mb2 [WIDTH][HEIGHT];
int ma2 [WIDTH][HEIGHT];

//struct utilizado para passar parâmetros na função pthread_create
struct range_matriz{
	int linhaInicial;
	int linhaFinal;
};
//pixels location reference
    //      +--------------+
    //      | x0 | x1 | x2 |
    //      |----|----|----|
    //      | x3 | x4 | x5 |
    //      |----|----|----|
    //      | x6 | x7 | x8 |
    //      +--------------+
void filtro(int linha, int coluna){
	int stencilR[3][3],stencilG[3][3],stencilB[3][3]; //stencil do componente correspondente 
	int sumR=0,sumG=0,sumB=0; //soma dos componente correspondente
	int avgR,avgG, avgB; // média do componente correspondente

	//iStencil,jStencil para percorrer a matriz stencil
	for(int iStencil=linha-1;iStencil<linha+2;iStencil++){
		if(linha-1==-1){
			printf("%d\n",iStencil-(linha-1));
		}
		for(int jStencil=coluna-1;jStencil<coluna+2;jStencil++){
			//a soma dentro dos indices é parar garantir que o indice esteja entre 0 e 2
			stencilR[iStencil-(linha-1)][jStencil-(coluna-1)] = mr[iStencil][jStencil];
			stencilG[iStencil-(linha-1)][jStencil-(coluna-1)] = mg[iStencil][jStencil];
			stencilB[iStencil-(linha-1)][jStencil-(coluna-1)] = mb[iStencil][jStencil];
			//faz a soma de cada componente
			sumR+=stencilR[iStencil-(linha-1)][jStencil-(coluna-1)]; 
			sumG+=stencilG[iStencil-(linha-1)][jStencil-(coluna-1)];
			sumB+=stencilB[iStencil-(linha-1)][jStencil-(coluna-1)];

		}
	}
	
	avgR=sumR/9;
	avgG=sumG/9;
	avgB=sumB/9;

	//escrita na matriz resultante
	mr2[linha][coluna]=avgR;
	mg2[linha][coluna]=avgG;
	mb2[linha][coluna]=avgB;
	ma2[linha][coluna]=ma[linha][coluna];

}
void *smooth(void *pos){
	//desestrutura a entrada para pegar as linhas da thread
	int linhaInicial =((struct range_matriz *) pos)->linhaInicial;
	int linhaFinal =((struct range_matriz *) pos)->linhaFinal;
	//iMatriz,jMatriz para percorrer a matriz
	for(int iMatriz=linhaInicial; iMatriz<=linhaFinal; iMatriz++){
		//mudar para fazer cada thread chamar uma funcao que cuidara dos stencils do elemento central
		for(int jMatriz=1;jMatriz<512;jMatriz++){
			filtro(iMatriz,jMatriz);		
		}
	}			
}  


int 
main(int argc, char **argv)
{
	int i,j;
	int fdi, fdo;
	int nlin = 0;
	int ncol = 0;
	char name[128];
	int status;
	pthread_t threads[NTHREADS];
	// variáveis para medida do tempo
	struct timeval inic,fim;
	struct rusage r1, r2;

	if(argc<2) {
		printf("Uso: %s nome_arquivo\n",argv[0]);
		exit(0);
	}
	if((fdi=open(argv[1],O_RDONLY))==-1) {
		printf("Erro na abertura do arquivo %s\n",argv[1]);
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
	nlin=ncol=512;

	// zerar as matrizes (4 bytes, mas usaremos 1 por pixel)
	// void *memset(void *s, int c, size_t n);
	//matriz original recebe 2 colunas e linhas adicionais para tratamento da borda
	memset(mr,0,(nlin+2)*(ncol+2)*sizeof(int));
	memset(mg,0,(nlin+2)*(ncol+2)*sizeof(int));
	memset(mb,0,(nlin+2)*(ncol+2)*sizeof(int));
	memset(ma,0,(nlin+2)*(ncol+2)*sizeof(int));

	memset(mr2,0,(nlin)*(ncol)*sizeof(int));
	memset(mg2,0,(nlin)*(ncol)*sizeof(int));
	memset(mb2,0,(nlin)*(ncol)*sizeof(int));
	memset(ma2,0,(nlin)*(ncol)*sizeof(int));

	// (ao menos) 2 abordagens: 
	// - ler pixels byte a byte, colocando-os em matrizes separadas
	//	- ler pixels (32bits) e depois usar máscaras e rotações de bits para o processamento.

	// ordem de leitura dos bytes (componentes do pixel) depende se o formato
	// é little ou big endian
	// Assumindo little endian
	for(i=1;i<nlin+1;i++) {
		for(j=1;j<ncol+1;j++) {
			read(fdi,&mr[i][j],1);
			read(fdi,&mg[i][j],1);
			read(fdi,&mb[i][j],1);
			read(fdi,&ma[i][j],1);
		}
	}
	close(fdi);

	// aplicar filtro (estêncil)
	// repetir para mr2, mg2, mb2, ma2
	// obtém tempo e consumo de CPU antes da aplicação do filtro
/* 	gettimeofday(&inic,0);
	getrusage(RUSAGE_SELF, &r1); */
	//aplicar filtro paralelizado aqui
	//cria uma estrutura que contem a linha inicial e final que uma thread deve operar
	struct range_matriz *thread_range = (struct range_matriz*)malloc(sizeof(struct range_matriz)*NTHREADS);

	 // cria uma thread para cada elemento da matriz desconsiderando os elementos da borda
	for(int i=0;i<NTHREADS;i++){
		//pega as linhas através do num_linhas/num_threads
		//tratar divisoes nao inteiras
		(thread_range+i)->linhaInicial=(i*((HEIGHT)/NTHREADS)); //printf("ELEMENTO %d", (i+1)*64);
		(thread_range+i)->linhaFinal=(i+1)*((HEIGHT)/NTHREADS);
		//printf("LINHA INICIAL %d,  LINHA FINAL %d\n", (thread_range+i)->linhaInicial, (thread_range+i)->linhaFinal);
		//criação das threads
		status = pthread_create(&threads[i], NULL, smooth, (void *)(thread_range+i));
		if (status) {
			//strerror_r(status,err_msg,LEN);
			printf("Falha da criacao da thread");
			exit(EXIT_FAILURE);
		}
	}
	// espera threads retornarem
	for (int t=0; t<NTHREADS; t++) {
		//printf("%d\n",t);
		status = pthread_join(threads[t], NULL);
		if (status) {
			//strerror_r(status,err_msg,LEN);
			printf("Erro em pthread_join");
			exit(EXIT_FAILURE);
		}
	}

	// obtém tempo e consumo de CPU depois da aplicação do filtro
	/* gettimeofday(&fim,0);
	getrusage(RUSAGE_SELF, &r2);
	printf("\nElapsed time:%f sec\tUser time:%f sec\tSystem time:%f sec\n",
	(fim.tv_sec+fim.tv_usec/1000000.) - (inic.tv_sec+inic.tv_usec/1000000.),
	(r2.ru_utime.tv_sec+r2.ru_utime.tv_usec/1000000.) - (r1.ru_utime.tv_sec+r1.ru_utime.tv_usec/1000000.),
	(r2.ru_stime.tv_sec+r2.ru_stime.tv_usec/1000000.) - (r1.ru_stime.tv_sec+r1.ru_stime.tv_usec/1000000.));
 */

	// tratar: linhas 0, 1, n, n-1; colunas 0,1,n,n-1
	// for

	// gravar imagem resultante
	sprintf(name,"%s.new",argv[1]);	
	fdo=open(name, O_WRONLY| O_CREAT);

/*
	write(fdo,&ncol,2);
	write(fdo,&nlin,2);
*/

	for(i=0;i<nlin;i++) {
		for(j=0;j<ncol;j++) {
			write(fdo,&mr2[i][j],1);
			write(fdo,&mg2[i][j],1);
			write(fdo,&mb2[i][j],1);
			write(fdo,&ma2[i][j],1);
		}
	}
	close(fdo);
	
	return 0;
}

