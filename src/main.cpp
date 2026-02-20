#include <iostream>
#include <fstream>
#include <random>
#include <cstring>
#include <vector>
#include <ctime>

using namespace std;

#define JoinCondition 2

bool    insertProfessor(string, unsigned, string);
bool    insertStudent(string, unsigned, float, unsigned);
bool    readStudent(unsigned, unsigned*, string*, float*, unsigned*);
bool    searchStudent(unsigned, string *, float*, unsigned*);
bool    join(string*, string*, unsigned);


int main() {

    unsigned        numberStudents, numberProfessors, numberID, numberRank;
    clock_t         t_start, t_end;

    ifstream        prof_ifs("professor.txt", ios::in);
    ifstream        stud_ifs("student.txt", ios::in);
    ifstream        rank_ifs("rankQuery.txt", ios::in);
    ifstream        ID_ifs("IDQuery.txt", ios::in);

    if(prof_ifs.fail()) {
        cerr<<"Professor.txt Open Error"<<endl;
        exit(1);
    }

    if(stud_ifs.fail()) {
        cerr<<"Student.txt Open Error"<<endl;
        exit(1);
    }

    if(rank_ifs.fail()) {
        cerr<<"rank_query.txt Open Error"<<endl;
        exit(1);
    }


    if(ID_ifs.fail()) {
        cerr<<"IDQuery.txt Open Error"<<endl;
        exit(1);
    }


    string      name;
    string      dept;
    unsigned    ID;
    float       score;
    unsigned    advisorID;
    int         n;
    string      pname, sname;


    prof_ifs>>numberProfessors;
    stud_ifs>>numberStudents;
    rank_ifs>>numberRank;
    ID_ifs>>numberID;

    t_start=clock();

    for(unsigned i=0;i<numberProfessors;i++) {
        prof_ifs>>name>>ID>>dept;

        if(insertProfessor(name,ID,dept)!=true) {
            cerr<<"Professor Insert Error"<<endl;
            exit(1);
        }
    }

    for(unsigned i=0;i<numberStudents;i++) {
        stud_ifs>>name>>ID>>score>>advisorID;
        if(insertStudent(name,ID,score,advisorID)!=true) {
            cerr<<"Student Insert Error"<<endl;
            exit(1);
        }
    }

    for(unsigned i=0;i<numberRank;i++) {
        rank_ifs>>n;
        if(readStudent(n,&ID, &name,&score,&advisorID)!=true) {
            cerr<<"Student Read Error"<<endl;
            exit(1);
        }
    }

    for(unsigned i=0;i<numberID;i++) {
        ID_ifs>>ID;
        if(searchStudent(ID,&name,&score,&advisorID)!=true) {
            cerr<<"Student Search Error"<<endl;
            exit(1);
        }
    }

    if(join(&pname, &sname, JoinCondition)!=true) {
            cerr<<"Join Error"<<endl;
            exit(1);
    }

    t_end=clock();

    prof_ifs.close();
    stud_ifs.close();
    rank_ifs.close();
    ID_ifs.close();

    double duration=(double)(t_end-t_start)/CLOCKS_PER_SEC;
    cout<<"Execution time: "<<duration<<endl;

    return 0;
}

// the functions below are dummy test functions. Replace them with your program
bool    insertProfessor(string a, unsigned b, string c) { return true; }
bool    insertStudent(string a, unsigned b, float c, unsigned d) { return true; }
bool    searchStudent(unsigned ID, string* name, float* score, unsigned* advisorID){
    *name="abc"; *score=90.5; *advisorID=1111; return true;
}
bool    readStudent(unsigned n, unsigned* ID, string* name, float* score, unsigned* advisorID) {
    *ID=1234; *name="abc"; *score=90.5; *advisorID=1111; return true;
}
bool    join(string* pname, string* sname, unsigned n) { *pname="abc"; *sname="def"; return true;}
