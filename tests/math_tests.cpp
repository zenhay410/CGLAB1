#include "../source/scene_math.hpp"
#include <iostream>
#include <stdexcept>
#include <map>
#include <algorithm>
void require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
bool near(float a,float b){return std::abs(a-b)<1e-4f;}
int main(){
    try {
        std::map<std::pair<int,int>,int> edges;
        for(size_t i=0;i<scene::indices.size();i+=3){
            glm::vec3 p[3];
            for(int j=0;j<3;++j){int k=scene::indices[i+j];require(k<8,"Index outside vertex buffer");
                p[j]={scene::vertices[k][0],scene::vertices[k][1],scene::vertices[k][2]};
                int l=scene::indices[i+(j+1)%3];edges[std::minmax(k,l)]++;
            }
            require(glm::dot(glm::cross(p[1]-p[0],p[2]-p[0]),(p[0]+p[1]+p[2])/3.f)>0,"Incorrect triangle winding");
        }
        for(auto [e,count]:edges)require(count==2,"Cube mesh is not closed");
        for(bool ortho:{false,true}){
            auto p=scene::projection(ortho,1.5f,50,4);
            auto n=p*glm::vec4(0,0,-.1f,1),f=p*glm::vec4(0,0,-100,1);
            require(near(n.z/n.w,0)&&near(f.z/f.w,1),"Incorrect Vulkan depth range");
            require(p[1][1]<0,"Missing Vulkan Y correction");
            auto a=p*glm::vec4(1,0,-3,1),b=p*glm::vec4(1,0,-6,1);
            require(ortho ? near(a.x/a.w,b.x/b.w):near(a.x/a.w,2*b.x/b.w),"Incorrect projection distance behavior");
        }
        auto q=scene::model({1,2,3},{0,0,90},{2,1,1})*glm::vec4(1,0,0,1);
        require(near(q.x,1)&&near(q.y,4)&&near(q.z,3),"Incorrect transform order");
        auto a=scene::path(.7f,2,1,.2f),b=scene::path(.7f+glm::two_pi<float>(),2,1,.2f);
        require(glm::length(a-b)<1e-4f,"Trajectory is not periodic");
        require(near(a.x*a.x+a.z*a.z,4),"Incorrect trajectory radius");
        std::cout<<"PASS: closed cube, winding, Vulkan projections, transform order, trajectory\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
