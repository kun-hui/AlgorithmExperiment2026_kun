#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

// 最大打印选中物品数
#define MAX_ITEMS_TO_PRINT 50
#define INF 1e100

// ===================== 数据结构 =====================

// 物品结构体
typedef struct {
    int id;         // 物品编号
    int w;          // 物品重量
    double v;       // 物品价值
    double ratio;   // 价值/重量比
} Item;

// 背包结果结构体
typedef struct {
    double bestValue;        // 最大价值
    long long bestWeight;    // 总重量
    unsigned char *chosen;   // 被选中的物品标记数组
    int n;                   // 物品总数
} KnapResult;

// ===================== 全局变量 =====================
static unsigned long long cmp_count = 0ULL; // 排序比较次数
static FILE *subproblem_file = NULL;        // 记录分治法子问题文件
static int record_subproblem = 0;          // 是否记录子问题

// ===================== 工具函数 =====================

// 获取当前时间毫秒
long long now_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000LL + tv.tv_usec / 1000;
}

// 生成随机整数数组
void gen_int_array(int *a, int n, unsigned int seed) {
    srand(seed);
    for (int i = 0; i < n; ++i)
        a[i] = rand();
}

// 数组复制
void copy_array(int *dst, int *src, int n) {
    memcpy(dst, src, sizeof(int) * n);
}

// ===================== 排序算法 =====================

// 冒泡排序
void bubble_sort(int *a, int n) {
    cmp_count = 0ULL;
    for (int i = 0; i < n - 1; ++i)
        for (int j = 0; j < n - i - 1; ++j) {
            cmp_count++;
            if (a[j] > a[j+1]) {
                int t = a[j]; a[j] = a[j+1]; a[j+1] = t;
            }
        }
}

// 合并排序辅助函数
void merge(int *a, int l, int m, int r, int *tmp) {
    int i = l, j = m + 1, k = l;
    while (i <= m && j <= r) {
        cmp_count++;
        if (a[i] <= a[j]) tmp[k++] = a[i++];
        else tmp[k++] = a[j++];
    }
    while (i <= m) tmp[k++] = a[i++];
    while (j <= r) tmp[k++] = a[j++];
    for (i = l; i <= r; ++i) a[i] = tmp[i];
}

// 合并排序递归
void merge_sort_rec(int *a, int l, int r, int *tmp, int depth) {
    int n = r - l + 1;
    if (record_subproblem && subproblem_file)
        fprintf(subproblem_file, "merge,%d,%d\n", depth, n);
    if (l >= r) return;
    int m = l + (r - l) / 2;
    merge_sort_rec(a, l, m, tmp, depth + 1);
    merge_sort_rec(a, m + 1, r, tmp, depth + 1);
    merge(a, l, m, r, tmp);
}

void merge_sort(int *a, int n) {
    cmp_count = 0ULL;
    int *tmp = (int*)malloc(sizeof(int) * n);
    if (!tmp) { fprintf(stderr, "malloc failed\n"); exit(1); }
    merge_sort_rec(a, 0, n - 1, tmp, 0);
    free(tmp);
}

// 快速排序三数取中
int median3(int *a, int l, int r) {
    int m = l + (r - l)/2;
    if (a[l] > a[m]) { int t = a[l]; a[l] = a[m]; a[m] = t; }
    if (a[l] > a[r]) { int t = a[l]; a[l] = a[r]; a[r] = t; }
    if (a[m] > a[r]) { int t = a[m]; a[m] = a[r]; a[r] = t; }
    return m;
}

// 快速排序递归
void quick_sort_rec(int *a, int l, int r, int depth) {
    int n = r - l + 1;
    if (record_subproblem && subproblem_file && n > 0)
        fprintf(subproblem_file, "quick,%d,%d\n", depth, n);
    if (l >= r) return;
    int pidx = median3(a, l, r);
    int pivot = a[pidx];
    int t = a[pidx]; a[pidx] = a[r]; a[r] = t;
    int i = l;
    for (int j = l; j < r; ++j) {
        cmp_count++;
        if (a[j] <= pivot) {
            int x = a[i]; a[i] = a[j]; a[j] = x;
            i++;
        }
    }
    t = a[i]; a[i] = a[r]; a[r] = t;
    quick_sort_rec(a, l, i-1, depth+1);
    quick_sort_rec(a, i+1, r, depth+1);
}

void quick_sort(int *a, int n) {
    cmp_count = 0ULL;
    quick_sort_rec(a, 0, n-1, 0);
}

// 判断数组是否有序
int is_sorted(int *a, int n) {
    for (int i=1; i<n; i++)
        if (a[i-1] > a[i]) return 0;
    return 1;
}

// ===================== 背包问题 =====================

// 生成物品
void gen_items(Item *items, int n, unsigned int seed) {
    srand(seed);
    for (int i = 0; i < n; ++i) {
        items[i].id = i+1;
        items[i].w = 1 + rand()%100;
        items[i].v = 100.0 + (double)(rand()%90001)/100.0;
        items[i].ratio = items[i].v / items[i].w;
    }
}

// 按比率排序
int cmp_ratio_desc(const void *pa, const void *pb) {
    const Item *a = (const Item*)pa, *b = (const Item*)pb;
    if (a->ratio < b->ratio) return 1;
    if (a->ratio > b->ratio) return -1;
    return a->id - b->id;
}

// 初始化背包结果
KnapResult make_result(int n) {
    KnapResult r;
    r.bestValue = 0.0; r.bestWeight = 0; r.n = n;
    r.chosen = (unsigned char*)calloc(n,1);
    if (!r.chosen) { fprintf(stderr,"calloc failed\n"); exit(1);}
    return r;
}

void free_result(KnapResult *r) {
    free(r->chosen); r->chosen=NULL;
}

// ===================== 贪心法 =====================
KnapResult greedy_knap(Item *items, int n, int C) {
    Item *arr = (Item*)malloc(sizeof(Item)*n);
    memcpy(arr, items, sizeof(Item)*n);
    qsort(arr,n,sizeof(Item),cmp_ratio_desc);
    KnapResult res = make_result(n);
    for (int i=0;i<n;i++)
        if (res.bestWeight + arr[i].w <= C) {
            res.bestWeight += arr[i].w;
            res.bestValue += arr[i].v;
            res.chosen[arr[i].id-1]=1;
        }
    free(arr);
    return res;
}

// ===================== 动态规划 =====================
KnapResult dp_knap(Item *items,int n,int C) {
    double *dp=(double*)calloc(C+1,sizeof(double));
    int *take=(int*)malloc(sizeof(int)*n*(C+1));
    if (!dp || !take) { fprintf(stderr,"DP memory insufficient\n"); exit(2);}
    memset(take,0,sizeof(int)*n*(C+1));
    for (int i=0;i<n;i++){
        int w=items[i].w; double v=items[i].v;
        for (int c=C;c>=w;c--){
            double cand = dp[c-w]+v;
            if (cand>dp[c]) { dp[c]=cand; take[i*(C+1)+c]=1; }
        }
    }
    KnapResult res=make_result(n); res.bestValue=dp[C];
    int c=C;
    for (int i=n-1;i>=0;i--){
        if (take[i*(C+1)+c]){
            res.chosen[i]=1;
            res.bestWeight+=items[i].w;
            c-=items[i].w;
        }
    }
    free(dp); free(take); return res;
}

// ===================== 蛮力法 =====================
KnapResult brute_knap(Item *items,int n,int C){
    if (n>25) { fprintf(stderr,"Brute force limited to n<=25\n"); exit(3);}
    KnapResult res=make_result(n);
    unsigned long long total=1ULL<<n;
    for (unsigned long long mask=0; mask<total; mask++){
        int w=0; double v=0.0;
        for (int i=0;i<n;i++) if (mask&(1ULL<<i)){ w+=items[i].w; v+=items[i].v;}
        if (w<=C && v>res.bestValue){
            res.bestValue=v; res.bestWeight=w;
            memset(res.chosen,0,n);
            for (int i=0;i<n;i++) if (mask&(1ULL<<i)) res.chosen[i]=1;
        }
    }
    return res;
}

// ===================== 回溯法 =====================
static Item *bt_items; static int bt_n,bt_C;
static double bt_best; static int bt_weight;
static unsigned char *bt_x,*bt_bestx;

double bound_value(int idx,int weight,double value){
    int c=bt_C-weight; double b=value;
    for (int i=idx;i<bt_n && c>0;i++){
        if (bt_items[i].w<=c){ c-=bt_items[i].w; b+=bt_items[i].v;}
        else { b+=bt_items[i].ratio*c; break;}
    }
    return b;
}

void bt_rec(int idx,int weight,double value){
    if (idx==bt_n){
        if (value>bt_best){ bt_best=value; bt_weight=weight; memcpy(bt_bestx,bt_x,bt_n);}
        return;
    }
    if (weight+bt_items[idx].w<=bt_C){
        bt_x[idx]=1;
        bt_rec(idx+1,weight+bt_items[idx].w,value+bt_items[idx].v);
        bt_x[idx]=0;
    }
    if (bound_value(idx+1,weight,value)>bt_best) bt_rec(idx+1,weight,value);
}

KnapResult backtrack_knap(Item *items,int n,int C){
    if (n>2000){ fprintf(stderr,"Backtracking demo caps n<=2000\n"); exit(4);}
    Item *arr=(Item*)malloc(sizeof(Item)*n); memcpy(arr,items,sizeof(Item)*n);
    qsort(arr,n,sizeof(Item),cmp_ratio_desc);
    bt_items=arr; bt_n=n; bt_C=C; bt_best=0.0; bt_weight=0;
    bt_x=(unsigned char*)calloc(n,1); bt_bestx=(unsigned char*)calloc(n,1);
    bt_rec(0,0,0.0);
    KnapResult res=make_result(n); res.bestValue=bt_best; res.bestWeight=bt_weight;
    for (int i=0;i<n;i++) if (bt_bestx[i]) res.chosen[arr[i].id-1]=1;
    free(arr); free(bt_x); free(bt_bestx); return res;
}

// ===================== 输出函数 =====================
void print_knap_result(const char *name, KnapResult *r) {
    printf("%s total_value=%.2f total_weight=%lld selected_ids=", name,r->bestValue,r->bestWeight);
    int printed=0;
    for (int i=0;i<r->n;i++)
        if (r->chosen[i]){
            if (printed<MAX_ITEMS_TO_PRINT) printf("%d ",i+1);
            printed++;
        }
    if (printed>MAX_ITEMS_TO_PRINT) printf("...(%d items)",printed);
    printf("\n");
}

// 使用说明
void usage(){
    puts("Usage:\n  ./exp sort n seed [subproblem.csv]\n  ./exp knap alg n C seed\nalg=greedy|dp|brute|backtrack");
}

// ===================== 主函数入口 =====================
int main(int argc,char **argv){
    if (argc<2){ usage(); return 0;}

    if (strcmp(argv[1],"sort")==0){
        if (argc<4){ usage(); return 1;}
        int n=atoi(argv[2]); unsigned int seed=(unsigned int)atoi(argv[3]);
        if (argc>=5){
            subproblem_file=fopen(argv[4],"w");
            record_subproblem=1;
            fprintf(subproblem_file,"algorithm,depth,size\n");
        }
        int *base=(int*)malloc(sizeof(int)*n),*a=(int*)malloc(sizeof(int)*n);
        gen_int_array(base,n,seed);
        copy_array(a,base,n); long long st=now_ms(); bubble_sort(a,n); long long ed=now_ms();
        printf("bubble,n=%d,comparisons=%llu,time_ms=%lld,sorted=%d\n",n,cmp_count,ed-st,is_sorted(a,n));
        copy_array(a,base,n); st=now_ms(); merge_sort(a,n); ed=now_ms();
        printf("merge,n=%d,comparisons=%llu,time_ms=%lld,sorted=%d\n",n,cmp_count,ed-st,is_sorted(a,n));
        copy_array(a,base,n); st=now_ms(); quick_sort(a,n); ed=now_ms();
        printf("quick,n=%d,comparisons=%llu,time_ms=%lld,sorted=%d\n",n,cmp_count,ed-st,is_sorted(a,n));
        free(base); free(a); if (subproblem_file) fclose(subproblem_file);
    } else if (strcmp(argv[1],"knap")==0){
        if (argc<6){ usage(); return 1;}
        const char *alg=argv[2]; int n=atoi(argv[3]); int C=atoi(argv[4]); unsigned int seed=(unsigned int)atoi(argv[5]);
        Item *items=(Item*)malloc(sizeof(Item)*n); gen_items(items,n,seed);
        long long st=now_ms(); KnapResult r;
        if (strcmp(alg,"greedy")==0) r=greedy_knap(items,n,C);
        else if (strcmp(alg,"dp")==0) r=dp_knap(items,n,C);
        else if (strcmp(alg,"brute")==0) r=brute_knap(items,n,C);
        else if (strcmp(alg,"backtrack")==0) r=backtrack_knap(items,n,C);
        else { usage(); free(items); return 1;}
        long long ed=now_ms(); print_knap_result(alg,&r); printf("time_ms=%lld\n",ed-st);
        free_result(&r); free(items);
    } else usage();

    return 0;
}