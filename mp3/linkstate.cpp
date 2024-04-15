#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
#include <vector>
using namespace std;

const int MAX_SIZE = 6400;
const int INF = 65535;// unreachable
const int NEXT_HOP = 3;
const int MIN_COST = 2;
const int PRE_VEX = 1;
const int DEST_VEX = 0;
typedef int VertexType;
typedef int EdgeType;
typedef std::vector<std::vector<int> > ForwardTable;
//structure for graph
typedef struct {
    set<VertexType> vertexSet;
    vector<VertexType> isolated_vertex;
    vector<vector<EdgeType> > arc;
    vector<ForwardTable> forwardTables;
    int numVertexes, numEdges, numIsolated;    //当前图中顶点数和边数

}Graph;

typedef struct{
    VertexType source;
    VertexType dest;
    string content; //message content
}Message;

void swap(VertexType &a, VertexType &b){
    int temp = a;
    a = b;
    b = temp;
}

int Partition(VertexType *vertex, VertexType left, VertexType right){
    int i = left;
    int j = right;
    while (i < j){
        while (vertex[j] >= vertex[left] && i < j)
            j --;
        while (vertex[i] <= vertex[left] && i < j)
            i ++;
        swap(vertex[i],vertex[j]);
    }
    swap(vertex[left],vertex[i]);
    return i;
}


void QuickSort(VertexType *vertex, VertexType left, VertexType right){
    if (left >= right)
        return;
    int pivot = Partition(vertex, left, right);
    QuickSort(vertex, left, pivot - 1);
    QuickSort(vertex, pivot+1, right);
}

int AddVertex(Graph *graph, VertexType vex){
    //if error return 0
    if(graph->numVertexes >= MAX_SIZE)
        return 0;
    if(graph->vertexSet.find(vex) != graph->vertexSet.end())
        return 1;//existed
    graph->vertexSet.insert(vex);
    graph->numVertexes++;
    return 1;//added successfully
}

int AddEdge(Graph *graph, VertexType source, VertexType dest, EdgeType edge){
    if(graph->arc[source][dest] != INF)
        return 0;//existed
    graph->arc[source][dest] = edge;
    graph->arc[dest][source] = edge;
    graph->numEdges++;
    return 1;//added successfully

}

void CreateGraph(Graph *graph, const string& topofile){
    ifstream ifs(topofile);
    //read the file into an array
    int topo[MAX_SIZE][3];
    int topo_num = 0;
    //先一次读完文件
    if (ifs.is_open()){
        string line;
        while(getline(ifs, line)){
            stringstream ss;
            ss << line;
            if(!ss.eof()){
                //读取三个数
                int source, dest, cost;
                ss >> source >> dest >> cost;
                topo[topo_num][0] = source;
                topo[topo_num][1] = dest;
                topo[topo_num][2] = cost;
            }
            topo_num++;
        }
        ifs.close();
    }
    else{
        cout << "Failed to open file!" << endl;
    }
    graph->numVertexes = 0;
    graph->numEdges = 0;
    graph->numIsolated = 0;

    // 动态分配 isolated_vertex 和 arc 的内存
    graph->isolated_vertex.resize(topo_num);
    graph->arc.resize(topo_num + 10, std::vector<EdgeType>(topo_num + 10, 0));

    //initialize the arc matrix, allocate memory for the matrix as topo_num
    for (int i = 0; i < topo_num + 10; i++){
        for (int j = 0; j < topo_num + 10; j++){
            if (i == j)
                graph->arc[i][j] = 0;
            else
                graph->arc[i][j] = INF;
        }
    }
    //add vertex and edge
    for (int i = 0; i < topo_num; i++){
        int flag_vertex1 = AddVertex(graph, topo[i][0]);
        int flag_vertex2 = AddVertex(graph, topo[i][1]);

        if (flag_vertex1 == 1 && flag_vertex2 == 1){
            AddEdge(graph, topo[i][0], topo[i][1], topo[i][2]);
        }
    }
    //update the forward table
    graph->forwardTables.resize(graph->numVertexes+10, std::vector<std::vector<int> >(graph->numVertexes+10, std::vector<int>(4, INF)));
}

void dijkstra(Graph *graph, VertexType source){
    if(graph->vertexSet.find(source) == graph->vertexSet.end())
        return;
    //create forwarding table
    ForwardTable forward;
    forward.resize(graph->numVertexes + 10, std::vector<EdgeType>(4, INF));

    for(set<VertexType>::iterator iter = graph->vertexSet.begin(); iter != graph->vertexSet.end();iter ++){
        VertexType dest = *iter;
        forward[dest][DEST_VEX] = INF;// vertex
        forward[dest][PRE_VEX] = INF;// pre vertex
        forward[dest][MIN_COST] = INF;// min cost from source->vertex
        forward[dest][NEXT_HOP] = INF;// next hop
    }
    VertexType cur = INF;//current vertex
    EdgeType min = INF;
    set<VertexType> path;

    for (set<VertexType>::iterator it = graph->vertexSet.begin(); it != graph->vertexSet.end();it ++){
        VertexType dest;
        dest = *it;
        forward[dest][DEST_VEX] = dest;
        if(dest!=source){
            path.insert(dest);
        }
        VertexType cost = graph->arc[source][dest];
        forward[dest][MIN_COST] = graph->arc[source][dest];
        if (cost != INF)
            forward[dest][PRE_VEX] = source;
        if (cost < min && cost != 0){
            cur = dest;
            min = cost;
        }
    }
    // starting to forward
    //calculate dist of every next to other and update the table
    VertexType min_vex = cur;
    while(!path.empty()){
        path.erase(min_vex);
        cur = min_vex;
        min = INF;
        set<VertexType>::iterator iter;//iter for path
        for(iter = path.begin(); iter != path.end();iter ++){
            VertexType dest = *iter;
            if (graph->arc[cur][dest] + forward[cur][MIN_COST] < forward[dest][MIN_COST]){
                //更新
                forward[dest][PRE_VEX] = cur;
                forward[dest][MIN_COST] = graph->arc[cur][dest] + forward[cur][MIN_COST];
            }
            //tie breaking
            if (graph->arc[cur][dest] + forward[cur][MIN_COST] == forward[dest][MIN_COST]){
                if (cur < forward[dest][PRE_VEX]){
                    //更新
                    forward[dest][PRE_VEX] = cur;
                    forward[dest][MIN_COST] = graph->arc[cur][dest] + forward[cur][MIN_COST];
                }
            }
            if (forward[dest][MIN_COST] < min){
                min = forward[dest][MIN_COST];
                min_vex = dest;
            }
        }
        // find min
    }
    //add next hop column for the final forwarding table
    for (set<VertexType>::iterator it = graph->vertexSet.begin(); it != graph->vertexSet.end();it ++){
        VertexType dest = *it;
        VertexType pre = forward[dest][PRE_VEX];
        forward[dest][NEXT_HOP] = dest;// next hop = pre
        while(pre!=source){
            // backtrace next hop of the root vertex
            forward[dest][NEXT_HOP] = pre;
            pre = forward[pre][PRE_VEX];

        }
    }
    //assign the forward table directly to the graph->forwardTables[source]
    for (set<VertexType>::iterator it = graph->vertexSet.begin(); it != graph->vertexSet.end();it ++){
        VertexType dest = *it;
        graph->forwardTables[source][dest][DEST_VEX] = forward[dest][DEST_VEX];
        graph->forwardTables[source][dest][PRE_VEX] = forward[dest][PRE_VEX];
        graph->forwardTables[source][dest][MIN_COST] = forward[dest][MIN_COST];
        graph->forwardTables[source][dest][NEXT_HOP] = forward[dest][NEXT_HOP];
    }
}

//extract messages function
int ExtractMessages(const string& messagefile, Message messages[]){
    int message_num=0;
    ifstream ifs(messagefile);
    if (ifs.is_open()){
        string line;
        while(getline(ifs, line)){
            stringstream ss;
            ss << line;
            if(!ss.eof()){
                //读取2个数
                int source, dest;
                string temp,content;
                ss >> source >> dest;
                //读取message内容
                getline(ss, content);
                content = content.substr(1);
                messages[message_num].source =  source;
                messages[message_num].dest =  dest;
                messages[message_num].content =  content;
            }
            message_num++;
        }
        ifs.close();
    }
    else{
        cout << "Failed to open file!" << endl;
    }
    return message_num;
}
void sendMessages(Graph *graph, Message messages[], int message_num, ofstream& outputFile){
    for (int i = 0;i < message_num; i++){
        VertexType source = messages[i].source;
        VertexType dest = messages[i].dest;
        EdgeType cost = graph->forwardTables[source][dest][MIN_COST];
        if(cost==INF){
            // cout << "from " << source << " to " << dest << " cost " << "infinite hops unreachable ";
            outputFile << "from " << source << " to " << dest << " cost " << "infinite hops unreachable ";
        }
        else{
            // cout << "from " << source << " to " << dest << " cost " << cost << " hops ";
            outputFile << "from " << source << " to " << dest << " cost " << cost << " hops ";
            // print route
            while(source!= dest){
                // cout << source << " ";
                outputFile << source << " ";
                source = graph->forwardTables[source][dest][NEXT_HOP];
            }
        }

        // cout << "message " << messages[i].content<<endl;
        outputFile << "message " << messages[i].content<<endl;
    }
    // cout <<  endl;
    outputFile <<  endl;
}
void PrintForwardingTable(Graph *graph,ofstream& outputFile){
    //append to the file
    if (!outputFile.fail()) { // 检查文件是否打开成功
        set<VertexType> vertex;
        vertex = graph->vertexSet;
        if(graph->numIsolated > 0){
            for(int i = 0; i < graph->numIsolated;i++){
                vertex.insert( graph->isolated_vertex[i]);
            }
        }
        //print forwarding table entries
        for(set<VertexType>::iterator it = vertex.begin(); it != vertex.end();it ++){
            VertexType source = *it;
            ForwardTable forward = graph->forwardTables[source];
            for(set<VertexType>::iterator it2 = vertex.begin(); it2 != vertex.end();it2 ++){
                // <destination> <nexthop> <pathcost>
                VertexType v = *it2;
                if(forward[v][MIN_COST] != INF){
                    // cout << v <<" "<< forward[v][NEXT_HOP] <<" "<< forward[v][MIN_COST]<< endl;
                    outputFile << v <<" "<< forward[v][NEXT_HOP] <<" "<< forward[v][MIN_COST]<< endl;
                }
              }
            // cout << endl;
            outputFile << endl;
        }
    }
    else{
        cout << "Error opening file!" << std::endl;
    }
}

int extractChanges(const string& changesfile, VertexType changes[][3]){
    ifstream ifs(changesfile);
    int changes_num = 0;
    if (ifs.is_open()){
        string line;
        while(getline(ifs, line)){
            stringstream ss;
            ss << line;
            if(!ss.eof()){
                //读取3个数
                int source, dest, cost;
                ss >> source >> dest >> cost;
                changes[changes_num][0] = source;
                changes[changes_num][1] = dest;
                changes[changes_num][2] = cost;
            }
            changes_num ++ ;
        }
        ifs.close();
    }
    else{
        cout << "Failed to open file!" << endl;
    }
    return changes_num;
}

void RemoveUnreachableVex(Graph *graph){
    //we need to get rid of the unreachable ones
    //if one
    int isolated_num = 0;
    set<VertexType>::iterator it = graph->vertexSet.begin();
    while (it != graph->vertexSet.end()){
        //for each vex, if the edge to other except for itself is all INF the remove it
        bool unreachable = true;
        for(set<VertexType>::iterator it2 = graph->vertexSet.begin(); it2 != graph->vertexSet.end();it2 ++){
            if(graph->arc[*it][*it2]!=INF && *it!=*it2){
                unreachable = false;
                break;
            }
        }
        if (unreachable){
            //remove
            VertexType removedVex = *it;
            set<VertexType>::iterator temp = it;
            it++;// 获取下一个迭代器
            graph->vertexSet.erase(temp); // 删除当前元素及其后一个元素
            graph->numVertexes--;
            //make all to INF
            for(set<VertexType>::iterator it2 = graph->vertexSet.begin(); it2 != graph->vertexSet.end();it2 ++){
                VertexType dest = *it2;
                graph->arc[removedVex][dest]  = INF;
                graph->arc[dest][removedVex]  = INF;
                graph->forwardTables[removedVex][dest][MIN_COST] = INF;
                graph->forwardTables[dest][removedVex][MIN_COST] = INF;
            }
            graph->isolated_vertex[isolated_num] = removedVex;
            isolated_num++;
        }
        else{
            it++;
        }
    }
    for (set<VertexType>::iterator it = graph->vertexSet.begin(); it != graph->vertexSet.end();it ++){
        //for each vex, if the edge to other except for itself is all INF the remove it
        bool unreachable = true;
        for(set<VertexType>::iterator it2 = graph->vertexSet.begin(); it2 != graph->vertexSet.end();it2 ++){
            if(graph->arc[*it][*it2]!=INF && *it!=*it2){
                unreachable = false;
                break;
            }
        }
        if (unreachable){
            //remove
            VertexType removedVex = *it;
            set<VertexType>::iterator nextIt = next(it); // 获取下一个迭代器
            graph->vertexSet.erase(it, nextIt); // 删除当前元素及其后一个元素
            graph->numVertexes--;
            //make all to INF
            for(set<VertexType>::iterator it2 = graph->vertexSet.begin(); it2 != graph->vertexSet.end();it2 ++){
                VertexType dest = *it2;
                graph->arc[removedVex][dest]  = INF;
                graph->arc[dest][removedVex]  = INF;
                graph->forwardTables[removedVex][dest][MIN_COST] = INF;
                graph->forwardTables[dest][removedVex][MIN_COST] = INF;
            }
            graph->isolated_vertex[isolated_num] = removedVex;
            isolated_num++;
        }
    }
    graph->numIsolated = isolated_num;
}

void ApplyChanges(Graph *graph,VertexType changes[]){
    int source = changes[0];
    int dest = changes[1];
    int cost = changes[2];
    if(cost == -999) {
        graph->arc[source][dest] = INF;
        graph->arc[dest][source] = INF;
    }
    else
    {
        graph->arc[source][dest] = cost;
        graph->arc[dest][source] = cost;
    }
}

void updateFowardTable(Graph *graph){
    for(set<VertexType>::iterator it = graph->vertexSet.begin(); it != graph->vertexSet.end();it ++){
        dijkstra(graph, *it);
    }
}

int main(int argc, char** argv) {
    printf("Number of arguments: %d", argc);
    cout<<'\n'<<endl;
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }
    //calculate the time of the program
    clock_t start, end;
    start = clock();
    Graph *graph = new Graph();
    CreateGraph(graph, argv[1]);

    ofstream outputFile("output.txt");
    // before changes : print table
    updateFowardTable(graph);
    PrintForwardingTable(graph, outputFile);

    //extract messages
    Message messages[MAX_SIZE];
    int message_num = ExtractMessages(argv[2], messages);
    //send message
    sendMessages(graph, messages, message_num, outputFile);
    //extract changes
    VertexType changes[MAX_SIZE][3];
    int change_num = extractChanges(argv[3], changes);
    //apply
    for(int i = 0;i < change_num;i++){
        ApplyChanges(graph,changes[i]);
        if(changes[i][2]==-999)
            RemoveUnreachableVex(graph);
        updateFowardTable(graph);
        PrintForwardingTable(graph, outputFile);
        sendMessages(graph, messages, message_num, outputFile);
    }
    outputFile.close();
    end = clock();
    cout << "Time: " << (double)(end - start) / CLOCKS_PER_SEC << "s" << endl;
    //release graph
    delete graph;
    return 0;
}
