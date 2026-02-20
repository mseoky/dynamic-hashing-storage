#include <iostream>
#include <fstream>
#include <random>
#include <cstring>
#include <vector>
#include <ctime>

using namespace std;

#define JoinCondition 2
#define BLOCK_SIZE 4096
static const unsigned PROF_BUCKETS = 256;
static const unsigned STUD_BUCKETS = 256;

#pragma pack(push, 1)
struct Student {
    unsigned ID;
    char name[20];
    float score;
    unsigned advisorID;
};

struct Professor {
    char name[20];
    unsigned ID;
    char dept[10];
};

struct AdvisorRecord {
    unsigned advisorID;
    unsigned count;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct StudentRecord {
    Student s;
    unsigned n;
};
#pragma pack(pop)

static const unsigned PROF_OVERHEAD = sizeof(unsigned) + sizeof(unsigned);
static const unsigned STUD_OVERHEAD = sizeof(unsigned) + sizeof(unsigned);
static const unsigned ADVISOR_OVERHEAD = sizeof(unsigned) + sizeof(unsigned);
static const unsigned PROF_CAPACITY = (BLOCK_SIZE - PROF_OVERHEAD) / sizeof(Professor);
static const unsigned STUD_CAPACITY = (BLOCK_SIZE - STUD_OVERHEAD) / sizeof(StudentRecord);
static const unsigned ADVISOR_CAPACITY = (BLOCK_SIZE - ADVISOR_OVERHEAD) / sizeof(AdvisorRecord);

// 메인 메모리에서 사용할 블록 3개
static char studentBlock[BLOCK_SIZE];
static char professorBlock[BLOCK_SIZE];
static char multipurposeBlock[BLOCK_SIZE];

static fstream profFile;
static fstream studFile;
static fstream profDirFile;
static fstream studDirFile;
static fstream advisorFile;
static fstream advisorDirFile;
static unsigned profGlobalDepth;
static unsigned studGlobalDepth;
static unsigned advisorGlobalDepth;
static unsigned profNextBucketId;
static unsigned studNextBucketId;
static unsigned advisorNextBucketId;
static unsigned nextStudentRank = 0;

bool    insertProfessor(string, unsigned, string);
bool    insertStudent(string, unsigned, float, unsigned);
bool    readStudent(unsigned, unsigned*, string*, float*, unsigned*);
bool    searchStudent(unsigned, string*, float*, unsigned*);
bool    join(string*, string*, unsigned);

// Extensible Dynamic Hashing
inline unsigned hash32(unsigned);
inline unsigned getDirectoryIndex(unsigned, unsigned);
void loadBucket(char*, fstream&, unsigned);
void storeBucket(const char*, fstream&, unsigned);
inline void readBucketHeader(const char*, unsigned&, unsigned&);
inline void writeBucketHeader(char*, unsigned, unsigned);
static void initializeDirectory(fstream&, bool isProf);
static void initializeAdvisorDirectory();
static void loadDirectory(fstream&, unsigned*&);
static void storeDirectory(fstream&);
static void splitProfessorBucket(unsigned);
static void splitStudentBucket(unsigned);
static void splitAdvisorBucket(unsigned);

static void openFile(fstream&, const string&);

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
        }
    }

    for(unsigned i=0;i<numberStudents;i++) {
        stud_ifs>>name>>ID>>score>>advisorID;
        if(insertStudent(name,ID,score,advisorID)!=true) {
            cerr<<"Student Insert Error"<<endl;
        }
    }

    for(unsigned i=0;i<numberRank;i++) {
        rank_ifs>>n;
        if(readStudent(n,&ID, &name,&score,&advisorID)!=true) {
            cerr<<"Student Read Error"<<endl;
        }
    }

    for(unsigned i=0;i<numberID;i++) {
        ID_ifs>>ID;
        if(searchStudent(ID,&name,&score,&advisorID)!=true) {
            cerr<<"Student Search Error"<<endl;
        }
    }

    if(join(&pname, &sname, JoinCondition)!=true) {
            cerr<<"Join Error"<<endl;
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

bool insertProfessor(string name, unsigned ID, string dept) {
    if (!profFile.is_open()) {
        openFile(profFile, "professor.dat");
        openFile(profDirFile, "profDir.dat");
        initializeDirectory(profDirFile, true);

        // 초기 버킷 2개 생성
        for (unsigned b = 0; b < 2; b++) {
            memset(professorBlock, 0, BLOCK_SIZE);
            writeBucketHeader(professorBlock, 1, 0);
            storeBucket(professorBlock, profFile, b);
        }
    }

    unsigned* dirArr;
    loadDirectory(profDirFile, dirArr);
    unsigned h = hash32(ID);
    unsigned idx = getDirectoryIndex(h, profGlobalDepth);
    unsigned bucketId = dirArr[idx];

    loadBucket(professorBlock, profFile, bucketId);
    unsigned localDepth, count;
    readBucketHeader(professorBlock, localDepth, count);

    // 중복 확인
    for (unsigned i = 0; i < count; i++) {
        Professor tmpP;
        memcpy(&tmpP, professorBlock + PROF_OVERHEAD + i * sizeof(Professor), sizeof(Professor));
        if (tmpP.ID == ID) return false;
    }

    // 버킷에 여유 공간이 있으면 삽입
    if (count < PROF_CAPACITY) {
        Professor newP;
        memset(newP.name, 0, 20);
        memcpy(newP.name, name.c_str(), min<size_t>(name.size(), 20));
        newP.ID = ID;
        memset(newP.dept, 0, 10);
        memcpy(newP.dept, dept.c_str(), min<size_t>(dept.size(), 10));
        memcpy(professorBlock + PROF_OVERHEAD + count * sizeof(Professor), &newP, sizeof(Professor));
        count++;
        writeBucketHeader(professorBlock, localDepth, count);
        storeBucket(professorBlock, profFile, bucketId);
        return true;
    }

    // 버킷 분할 필요
    if (localDepth < profGlobalDepth) {
        splitProfessorBucket(bucketId);
    } else {
        unsigned oldSize = (1u << profGlobalDepth);
        loadDirectory(profDirFile, dirArr);
        for (int i = int(oldSize - 1); i >= 0; i--) {
            dirArr[i + oldSize] = dirArr[i];
        }
        profGlobalDepth++;
        storeDirectory(profDirFile);
        splitProfessorBucket(bucketId);
    }

    return insertProfessor(name, ID, dept);
}

bool insertStudent(string name, unsigned ID, float score, unsigned advisorID) {
    // student 해시 초기화
    if (!studFile.is_open()) {
        openFile(studFile, "student.dat");
        openFile(studDirFile, "studDir.dat");
        initializeDirectory(studDirFile, false);

        for (unsigned b = 0; b < 2; b++) {
            memset(studentBlock, 0, BLOCK_SIZE);
            writeBucketHeader(studentBlock, 1, 0);
            storeBucket(studentBlock, studFile, b);
        }
    }
    // advisor 해시 초기화
    if (!advisorFile.is_open()) {
        openFile(advisorFile, "advisor.dat");
        openFile(advisorDirFile, "advisorDir.dat");
        initializeAdvisorDirectory();

        for (unsigned b = 0; b < 2; b++) {
            memset(studentBlock, 0, BLOCK_SIZE); // studentBlock을 advisor 버킷 초기화에 사용
            writeBucketHeader(studentBlock, 1, 0);
            storeBucket(studentBlock, advisorFile, b);
        }
    }

    // student 해시 삽입
    unsigned* dirArr;
    loadDirectory(studDirFile, dirArr);
    unsigned h = hash32(ID);
    unsigned idx = getDirectoryIndex(h, studGlobalDepth);
    unsigned bucketId = dirArr[idx];

    loadBucket(studentBlock, studFile, bucketId);
    unsigned localDepth, count;
    readBucketHeader(studentBlock, localDepth, count);

    // 중복 확인
    for (unsigned i = 0; i < count; i++) {
        StudentRecord tmpS;
        memcpy(&tmpS, studentBlock + STUD_OVERHEAD + i * sizeof(StudentRecord), sizeof(StudentRecord));
        if (tmpS.s.ID == ID) return false;
    }

    // 빈 공간에 삽입
    if (count < STUD_CAPACITY) {
        StudentRecord newS;
        newS.s.ID = ID;
        memset(newS.s.name, 0, 20);
        memcpy(newS.s.name, name.c_str(), min<size_t>(name.size(), 20));
        newS.s.score = score;
        newS.s.advisorID = advisorID;
        newS.n = nextStudentRank++;
        memcpy(studentBlock + STUD_OVERHEAD + count * sizeof(StudentRecord), &newS, sizeof(StudentRecord));
        count++;
        writeBucketHeader(studentBlock, localDepth, count);
        storeBucket(studentBlock, studFile, bucketId);
    } else {
        // 버킷 분할
        if (localDepth < studGlobalDepth) {
            splitStudentBucket(bucketId);
        } else {
            unsigned oldSize = (1u << studGlobalDepth);
            loadDirectory(studDirFile, dirArr);
            for (int i = int(oldSize - 1); i >= 0; i--) {
                dirArr[i + oldSize] = dirArr[i];
            }
            studGlobalDepth++;
            storeDirectory(studDirFile);
            splitStudentBucket(bucketId);
        }
        return insertStudent(name, ID, score, advisorID);
    }

    // advisor 해시 업데이트(studentBlock 재활용)
    unsigned* aDir;
    loadDirectory(advisorDirFile, aDir);
    unsigned ah = hash32(advisorID);
    unsigned aIdx = getDirectoryIndex(ah, advisorGlobalDepth);
    unsigned aBucketId = aDir[aIdx];

    loadBucket(studentBlock, advisorFile, aBucketId);
    unsigned aLocalDepth, aCount;
    readBucketHeader(studentBlock, aLocalDepth, aCount);

    bool found = false;
    for (unsigned i = 0; i < aCount; i++) {
        AdvisorRecord tmpA;
        memcpy(&tmpA, studentBlock + ADVISOR_OVERHEAD + i * sizeof(AdvisorRecord), sizeof(AdvisorRecord));
        if (tmpA.advisorID == advisorID) {
            tmpA.count++;
            memcpy(studentBlock + ADVISOR_OVERHEAD + i * sizeof(AdvisorRecord), &tmpA, sizeof(AdvisorRecord));
            writeBucketHeader(studentBlock, aLocalDepth, aCount);
            storeBucket(studentBlock, advisorFile, aBucketId);
            found = true;
            break;
        }
    }
    if (!found) {
        if (aCount < ADVISOR_CAPACITY) {
            AdvisorRecord newA;
            newA.advisorID = advisorID;
            newA.count = 1;
            memcpy(studentBlock + ADVISOR_OVERHEAD + aCount * sizeof(AdvisorRecord), &newA, sizeof(AdvisorRecord));
            aCount++;
            writeBucketHeader(studentBlock, aLocalDepth, aCount);
            storeBucket(studentBlock, advisorFile, aBucketId);
        } else {
            // advisor 버킷 분할
            if (aLocalDepth < advisorGlobalDepth) {
                splitAdvisorBucket(aBucketId);
            } else {
                unsigned oldSize = (1u << advisorGlobalDepth);
                loadDirectory(advisorDirFile, aDir);
                for (int i = int(oldSize - 1); i >= 0; i--) {
                    aDir[i + oldSize] = aDir[i];
                }
                advisorGlobalDepth++;
                storeDirectory(advisorDirFile);
                splitAdvisorBucket(aBucketId);
            }
            return true;
        }
    }

    return true;
}

bool searchStudent(unsigned ID, string* name, float* score, unsigned* advisorID) {
    if (!studFile.is_open()) return false;

    // hash값 이용 해서 해당 버킷만 순회
    unsigned* dirArr;
    loadDirectory(studDirFile, dirArr);
    unsigned h = hash32(ID);
    unsigned idx = getDirectoryIndex(h, studGlobalDepth);
    unsigned bucketId = dirArr[idx];

    loadBucket(studentBlock, studFile, bucketId);
    unsigned localDepth, count;
    readBucketHeader(studentBlock, localDepth, count);

    for (unsigned i = 0; i < count; i++) {
        StudentRecord tmpS;
        memcpy(&tmpS, studentBlock + STUD_OVERHEAD + i * sizeof(StudentRecord), sizeof(StudentRecord));
        if (tmpS.s.ID == ID) {
            *name = string(tmpS.s.name, strnlen(tmpS.s.name, 20));
            *score = tmpS.s.score;
            *advisorID = tmpS.s.advisorID;
            return true;
        }
    }
    return false;
}

bool readStudent(unsigned n, unsigned* ID, string* name, float* score, unsigned* advisorID) {
    if (!studFile.is_open()) return false;

    // 전체 버킷 순회
    for (unsigned bucketId = 0; bucketId < studNextBucketId; bucketId++) {
        loadBucket(studentBlock, studFile, bucketId);
        unsigned localDepth, count;
        readBucketHeader(studentBlock, localDepth, count);
        for (unsigned i = 0; i < count; i++) {
            StudentRecord tmpS;
            memcpy(&tmpS, studentBlock + STUD_OVERHEAD + i * sizeof(StudentRecord), sizeof(StudentRecord));
            if (tmpS.n == n) {
                *ID = tmpS.s.ID;
                *name = string(tmpS.s.name, strnlen(tmpS.s.name, 20));
                *score = tmpS.s.score;
                *advisorID = tmpS.s.advisorID;
                return true;
            }
        }
    }
    return false;
}

bool join(string* pName, string* sName, unsigned n) {
    if (!profFile.is_open() || !studFile.is_open() || !advisorFile.is_open()) return false;

    unsigned minProfID = 0xFFFFFFFF;
    // advisor dynamic hash 순회: count == n인 advisorID 찾기
    unsigned* aDir;
    loadDirectory(advisorDirFile, aDir);
    for (unsigned bucketId = 0; bucketId < advisorNextBucketId; bucketId++) {
        loadBucket(studentBlock, advisorFile, bucketId);
        unsigned localDepth, count;
        readBucketHeader(studentBlock, localDepth, count);
        for (unsigned i = 0; i < count; i++) {
            AdvisorRecord tmpA;
            memcpy(&tmpA, studentBlock + ADVISOR_OVERHEAD + i * sizeof(AdvisorRecord), sizeof(AdvisorRecord));
            if (tmpA.count == n) {
                if (tmpA.advisorID < minProfID) {
                    minProfID = tmpA.advisorID;
                }
            }
        }
    }
    if (minProfID == 0xFFFFFFFF) return false;

    // 최소 교수 ID에 해당하는 이름 가져오기
    unsigned* pDir;
    loadDirectory(profDirFile, pDir);
    unsigned ph = hash32(minProfID);
    unsigned pIdx = getDirectoryIndex(ph, profGlobalDepth);
    unsigned pBucketId = pDir[pIdx];
    loadBucket(professorBlock, profFile, pBucketId);
    unsigned pLocalDepth, pCount;
    readBucketHeader(professorBlock, pLocalDepth, pCount);
    for (unsigned i = 0; i < pCount; i++) {
        Professor tmpP;
        memcpy(&tmpP, professorBlock + PROF_OVERHEAD + i * sizeof(Professor), sizeof(Professor));
        if (tmpP.ID == minProfID) {
            *pName = string(tmpP.name, strnlen(tmpP.name, 20));
            break;
        }
    }

    // 해당 교수의 학생 중 ID가 가장 작은 학생 찾기
    unsigned minStuID = 0xFFFFFFFF;
    string minStuName;
    unsigned* sDir;
    loadDirectory(studDirFile, sDir);
    for (unsigned bucketId = 0; bucketId < studNextBucketId; bucketId++) {
        loadBucket(studentBlock, studFile, bucketId);
        unsigned sLocalDepth, sCount;
        readBucketHeader(studentBlock, sLocalDepth, sCount);
        for (unsigned i = 0; i < sCount; i++) {
            StudentRecord tmpS;
            memcpy(&tmpS, studentBlock + STUD_OVERHEAD + i * sizeof(StudentRecord), sizeof(StudentRecord));
            if (tmpS.s.advisorID == minProfID) {
                if (tmpS.s.ID < minStuID) {
                    minStuID = tmpS.s.ID;
                    minStuName = string(tmpS.s.name, strnlen(tmpS.s.name, 20));
                }
            }
        }
    }
    if (minStuID == 0xFFFFFFFF) return false;
    *sName = minStuName;
    return true;
}

inline unsigned hash32(unsigned key) {
    return static_cast<unsigned>(std::hash<unsigned>()(key));
}

inline unsigned getDirectoryIndex(unsigned hashVal, unsigned depth) {
    return hashVal & ((1u << depth) - 1);
}

void loadBucket(char* blockBuffer, fstream& fs, unsigned bucketId) {
    fs.seekg(bucketId * BLOCK_SIZE, ios::beg);
    fs.read(blockBuffer, BLOCK_SIZE);
}

void storeBucket(const char* blockBuffer, fstream& fs, unsigned bucketId) {
    fs.seekp(bucketId * BLOCK_SIZE, ios::beg);
    fs.write(blockBuffer, BLOCK_SIZE);
    fs.flush();
}

inline void readBucketHeader(const char* blockBuffer, unsigned& localDepth, unsigned& count) {
    memcpy(&localDepth, blockBuffer, sizeof(unsigned));
    memcpy(&count, blockBuffer + sizeof(unsigned), sizeof(unsigned));
}

inline void writeBucketHeader(char* blockBuffer, unsigned localDepth, unsigned count) {
    memcpy(blockBuffer, &localDepth, sizeof(unsigned));
    memcpy(blockBuffer + sizeof(unsigned), &count, sizeof(unsigned));
}

static void initializeDirectory(fstream& dirFile, bool isProf) {
    memset(multipurposeBlock, 0, BLOCK_SIZE);
    unsigned* dirArr = reinterpret_cast<unsigned*>(multipurposeBlock);
    dirArr[0] = 0;
    dirArr[1] = 1;
    if (isProf) {
        profGlobalDepth = 1;
        profNextBucketId = 2;
    } else {
        studGlobalDepth = 1;
        studNextBucketId = 2;
    }
    dirFile.seekp(0, ios::beg);
    dirFile.write(multipurposeBlock, BLOCK_SIZE);
    dirFile.flush();
}

static void initializeAdvisorDirectory() {
    memset(multipurposeBlock, 0, BLOCK_SIZE);
    unsigned* dirArr = reinterpret_cast<unsigned*>(multipurposeBlock);
    dirArr[0] = 0;
    dirArr[1] = 1;
    advisorGlobalDepth = 1;
    advisorNextBucketId = 2;
    advisorDirFile.seekp(0, ios::beg);
    advisorDirFile.write(multipurposeBlock, BLOCK_SIZE);
    advisorDirFile.flush();
}

static void loadDirectory(fstream& dirFile, unsigned*& dirArray) {
    dirFile.seekg(0, ios::beg);
    dirFile.read(multipurposeBlock, BLOCK_SIZE);
    dirArray = reinterpret_cast<unsigned*>(multipurposeBlock);
}

static void storeDirectory(fstream& dirFile) {
    dirFile.seekp(0, ios::beg);
    dirFile.write(multipurposeBlock, BLOCK_SIZE);
    dirFile.flush();
}

static void splitProfessorBucket(unsigned bucketId) {
    loadBucket(professorBlock, profFile, bucketId);
    unsigned oldLocalDepth, oldCount;
    readBucketHeader(professorBlock, oldLocalDepth, oldCount);
    // 임시 저장: studentBlock에 복사
    memcpy(studentBlock, professorBlock, BLOCK_SIZE);

    unsigned* dirArr;
    loadDirectory(profDirFile, dirArr);

    unsigned newLocalDepth = oldLocalDepth + 1;
    unsigned oldBucketId = bucketId;
    unsigned newBucketId = profNextBucketId++;

    // 두 개 버킷 초기화
    memset(professorBlock, 0, BLOCK_SIZE);
    writeBucketHeader(professorBlock, newLocalDepth, 0);
    storeBucket(professorBlock, profFile, oldBucketId);

    memset(professorBlock, 0, BLOCK_SIZE);
    writeBucketHeader(professorBlock, newLocalDepth, 0);
    storeBucket(professorBlock, profFile, newBucketId);

    // 디렉터리 갱신
    unsigned dirSize = (1u << profGlobalDepth);
    for (unsigned i = 0; i < dirSize; i++) {
        if (dirArr[i] == oldBucketId) {
            unsigned bit = (i >> oldLocalDepth) & 1u;
            dirArr[i] = (bit ? newBucketId : oldBucketId);
        }
    }
    storeDirectory(profDirFile);

    // 레코드 재삽입
    for (unsigned i = 0; i < oldCount; i++) {
        Professor rec;
        memcpy(&rec, studentBlock + PROF_OVERHEAD + i * sizeof(Professor), sizeof(Professor));
        unsigned h = hash32(rec.ID);
        unsigned idx = getDirectoryIndex(h, newLocalDepth);
        unsigned targetBucketId = dirArr[idx];

        loadBucket(professorBlock, profFile, targetBucketId);
        unsigned ld, cnt;
        readBucketHeader(professorBlock, ld, cnt);

        memcpy(professorBlock + PROF_OVERHEAD + cnt * sizeof(Professor), &rec, sizeof(Professor));
        cnt++;
        writeBucketHeader(professorBlock, ld, cnt);
        storeBucket(professorBlock, profFile, targetBucketId);
    }
}

static void splitStudentBucket(unsigned bucketId) {
    loadBucket(studentBlock, studFile, bucketId);
    unsigned oldLocalDepth, oldCount;
    readBucketHeader(studentBlock, oldLocalDepth, oldCount);
    // 임시 저장: professorBlock에 복사
    memcpy(professorBlock, studentBlock, BLOCK_SIZE);

    unsigned* dirArr;
    loadDirectory(studDirFile, dirArr);

    unsigned newLocalDepth = oldLocalDepth + 1;
    unsigned oldBucketId = bucketId;
    unsigned newBucketId = studNextBucketId++;

    // 두 개 버킷 초기화
    memset(studentBlock, 0, BLOCK_SIZE);
    writeBucketHeader(studentBlock, newLocalDepth, 0);
    storeBucket(studentBlock, studFile, oldBucketId);

    memset(studentBlock, 0, BLOCK_SIZE);
    writeBucketHeader(studentBlock, newLocalDepth, 0);
    storeBucket(studentBlock, studFile, newBucketId);

    // 디렉터리 갱신
    unsigned dirSize = (1u << studGlobalDepth);
    for (unsigned i = 0; i < dirSize; i++) {
        if (dirArr[i] == oldBucketId) {
            unsigned bit = (i >> oldLocalDepth) & 1u;
            dirArr[i] = (bit ? newBucketId : oldBucketId);
        }
    }
    storeDirectory(studDirFile);

    // 레코드 재삽입
    for (unsigned i = 0; i < oldCount; i++) {
        StudentRecord rec;
        memcpy(&rec, professorBlock + STUD_OVERHEAD + i * sizeof(StudentRecord), sizeof(StudentRecord));
        unsigned h = hash32(rec.s.ID);
        unsigned idx = getDirectoryIndex(h, newLocalDepth);
        unsigned targetBucketId = dirArr[idx];

        loadBucket(studentBlock, studFile, targetBucketId);
        unsigned ld, cnt;
        readBucketHeader(studentBlock, ld, cnt);

        memcpy(studentBlock + STUD_OVERHEAD + cnt * sizeof(StudentRecord), &rec, sizeof(StudentRecord));
        cnt++;
        writeBucketHeader(studentBlock, ld, cnt);
        storeBucket(studentBlock, studFile, targetBucketId);
    }
}

static void splitAdvisorBucket(unsigned bucketId) {
    // advisor bucket을 studentBlock에 읽어오기
    loadBucket(studentBlock, advisorFile, bucketId);
    unsigned oldLocalDepth, oldCount;
    readBucketHeader(studentBlock, oldLocalDepth, oldCount);
    // 임시 저장: professorBlock에 복사
    memcpy(professorBlock, studentBlock, BLOCK_SIZE);

    unsigned* dirArr;
    loadDirectory(advisorDirFile, dirArr);

    unsigned newLocalDepth = oldLocalDepth + 1;
    unsigned oldBucketId = bucketId;
    unsigned newBucketId = advisorNextBucketId++;

    // 두 개 버킷 초기화 (studentBlock 재사용)
    memset(studentBlock, 0, BLOCK_SIZE);
    writeBucketHeader(studentBlock, newLocalDepth, 0);
    storeBucket(studentBlock, advisorFile, oldBucketId);

    memset(studentBlock, 0, BLOCK_SIZE);
    writeBucketHeader(studentBlock, newLocalDepth, 0);
    storeBucket(studentBlock, advisorFile, newBucketId);

    // 디렉터리 갱신
    unsigned dirSize = (1u << advisorGlobalDepth);
    for (unsigned i = 0; i < dirSize; i++) {
        if (dirArr[i] == oldBucketId) {
            unsigned bit = (i >> oldLocalDepth) & 1u;
            dirArr[i] = (bit ? newBucketId : oldBucketId);
        }
    }
    storeDirectory(advisorDirFile);

    // 레코드 재삽입
    for (unsigned i = 0; i < oldCount; i++) {
        AdvisorRecord rec;
        memcpy(&rec, professorBlock + ADVISOR_OVERHEAD + i * sizeof(AdvisorRecord), sizeof(AdvisorRecord));
        unsigned h = hash32(rec.advisorID);
        unsigned idx = getDirectoryIndex(h, newLocalDepth);
        unsigned targetBucketId = dirArr[idx];

        loadBucket(studentBlock, advisorFile, targetBucketId);
        unsigned ld, cnt;
        readBucketHeader(studentBlock, ld, cnt);

        memcpy(studentBlock + ADVISOR_OVERHEAD + cnt * sizeof(AdvisorRecord), &rec, sizeof(AdvisorRecord));
        cnt++;
        writeBucketHeader(studentBlock, ld, cnt);
        storeBucket(studentBlock, advisorFile, targetBucketId);
    }
}

static void openFile(fstream& fs, const string& filename) {
    fs.open(filename, ios::in | ios::out | ios::binary);
    if (!fs.is_open()) {
        fs.clear();
        fs.open(filename, ios::out | ios::binary);
        fs.close();
        fs.open(filename, ios::in | ios::out | ios::binary);
    }
}
