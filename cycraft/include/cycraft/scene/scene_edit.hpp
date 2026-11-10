#pragma once
// Constrained native scene edits. Operator CLI paths never enter model proposals.
#include <simdjson.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace singular::scene {
inline std::string quote(const std::string& text) {
    constexpr char hex[]="0123456789abcdef";
    std::string result="\"";
    for(unsigned char c:text) {
        if(c=='"'||c=='\\') { result+='\\';result+=char(c); }
        else if(c<32) {result+="\\u00";result+=hex[c>>4];result+=hex[c&15];}
        else result+=char(c);
    }
    return result+'"';
}
inline std::string asset_instance_json(const std::string& id,const std::string& asset,
                                       const std::string& location,const std::string& yaw) {
    return "{\"id\":"+quote(id)+",\"kind\":\"asset\",\"asset\":"+quote(asset)+
        ",\"location\":"+location+",\"yaw_degrees\":"+yaw+"}";
}
namespace editing {
using Element=simdjson::dom::element;
constexpr size_t scene_limit=16*1024*1024, proposal_limit=64*1024;
constexpr size_t max_edits=64,max_set_fields=4,max_id_length=128;
constexpr int64_t first_frame=1,last_frame=999999;
constexpr std::string_view edit_schema="singular.scene_edit/1",instance_schema="singular.scene_edit/2";
constexpr size_t max_instances=64,max_objects=10000;
struct TargetRule {std::string_view name,group;unsigned mask;bool frames;};
inline constexpr std::array target_rules={TargetRule{"object","objects",1,false},
    TargetRule{"camera","cameras",2,true},TargetRule{"material","materials",4,false}};
struct FieldRule {
    std::string_view name;unsigned targets;bool vector;double low,high;
    bool pose;unsigned object_kinds;
};
// Native validation and emitted model grammar consume this single field/range table.
inline constexpr std::array field_rules={
    FieldRule{"location",3,true,-10000,10000,true,31},
    FieldRule{"yaw_degrees",1,false,-360,360,false,24},
    FieldRule{"scale",1,true,0.01,100,false,16},
    FieldRule{"target",2,true,-10000,10000,true,0},
    FieldRule{"lens_mm",2,false,1,500,false,0},
    FieldRule{"f_stop",2,false,0.7,64,false,0},
    FieldRule{"color",4,true,0,1,false,0},
    FieldRule{"roughness",4,false,0.001,1,false,0}};
inline constexpr std::array<std::string_view,5> object_kinds={"box","mesh","sphere","asset","blend_collection"};
inline void require(bool condition,const std::string& error) {
    if(!condition)throw std::runtime_error("SCENE_EDIT: "+error);
}
inline const TargetRule& target_rule(std::string_view value,bool group=false) {
    for(const auto& rule:target_rules)if((group?rule.group:rule.name)==value)return rule;
    throw std::runtime_error("SCENE_EDIT: unknown edit target");
}
inline const FieldRule& field_rule(std::string_view name,unsigned target) {
    for(const auto& rule:field_rules)if(rule.name==name&&(rule.targets&target))return rule;
    throw std::runtime_error("SCENE_EDIT: unsupported edit field");
}
inline std::string field_schema(const FieldRule& rule) {
    std::string numeric="{\"type\":\"number\",\"minimum\":"+std::to_string(rule.low)+
        ",\"maximum\":"+std::to_string(rule.high)+"}";
    return rule.vector?"{\"type\":\"array\",\"minItems\":3,\"maxItems\":3,\"items\":"+numeric+"}":numeric;
}
inline std::string edit_branch_schema(const TargetRule& target,bool framed) {
    std::string properties="{";bool first=true;
    for(const auto& field:field_rules)if((field.targets&target.mask)&&(!framed||field.pose)) {
        if(!first)properties+=',';first=false;
        properties+=quote(std::string(field.name))+":"+field_schema(field);
    }
    properties+='}';
    return "{\"type\":\"object\",\"additionalProperties\":false,\"required\":[\"target\",\"id\",\"set\""+
        std::string(framed?",\"frame\"":"")+"],\"properties\":{\"target\":{\"const\":"+quote(std::string(target.name))+"},"
        "\"id\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":"+std::to_string(max_id_length)+"},"
        "\"set\":{\"type\":\"object\",\"additionalProperties\":false,\"minProperties\":1,\"maxProperties\":"+
        std::to_string(max_set_fields)+",\"properties\":"+properties+"}"+
        (framed?",\"frame\":{\"type\":\"integer\",\"minimum\":"+std::to_string(first_frame)+
        ",\"maximum\":"+std::to_string(last_frame)+"}":"")+"}}";
}
inline std::string schema_json(unsigned version=1) {
    require(version==1||version==2,"unsupported schema version");
    std::string branches="[";bool first=true;
    for(const auto& target:target_rules) {
        if(!first)branches+=',';first=false;branches+=edit_branch_schema(target,false);
        if(target.frames)branches+=","+edit_branch_schema(target,true);
    }
    if(version==2) {
        const std::string identifier="{\"type\":\"string\",\"minLength\":1,\"maxLength\":"+std::to_string(max_id_length)+"}";
        const std::string instance="{\"type\":\"object\",\"additionalProperties\":false,\"required\":[\"id\",\"asset\",\"location\",\"yaw_degrees\"],\"properties\":{"
            "\"id\":"+identifier+",\"asset\":"+identifier+",\"location\":"+field_schema(field_rule("location",1))+
            ",\"yaw_degrees\":"+field_schema(field_rule("yaw_degrees",1))+"}}";
        return "{\"$schema\":\"https://json-schema.org/draft/2020-12/schema\",\"title\":\"Bounded native edits and catalog instances\","
            "\"description\":\"Existing host asset IDs only. Native validation rejects missing or ambiguous references and colliding object IDs. No paths, code, scale or physics.\","
            "\"type\":\"object\",\"additionalProperties\":false,\"required\":[\"schema\",\"edits\",\"add_instances\"],"
            "\"anyOf\":[{\"properties\":{\"edits\":{\"minItems\":1}}},{\"properties\":{\"add_instances\":{\"minItems\":1}}}],"
            "\"properties\":{\"schema\":{\"const\":"+quote(std::string(instance_schema))+"},\"edits\":{\"type\":\"array\",\"maxItems\":"+
            std::to_string(max_edits)+",\"items\":{\"anyOf\":"+branches+"]}},\"add_instances\":{\"type\":\"array\",\"maxItems\":"+
            std::to_string(max_instances)+",\"items\":"+instance+"}}}\n";
    }
    return "{\"$schema\":\"https://json-schema.org/draft/2020-12/schema\","
        "\"title\":\"Bounded native scene edit proposal\","
        "\"description\":\"Existing scene IDs and fields only. Native validation additionally checks adapter support, keyed camera poses, texture-driven materials, duplicate keys, overlapping edits and camera focus distance. No paths or code.\","
        "\"type\":\"object\",\"additionalProperties\":false,\"required\":[\"schema\",\"edits\"],\"properties\":{"
        "\"schema\":{\"const\":"+quote(std::string(edit_schema))+"},\"edits\":{\"type\":\"array\",\"minItems\":1,\"maxItems\":"+
        std::to_string(max_edits)+",\"items\":{\"anyOf\":"+branches+"]}}}}\n";
}
struct Descriptor {
    int fd;
    explicit Descriptor(int value):fd(value) {}
    Descriptor(const Descriptor&)=delete;
    ~Descriptor(){if(fd>=0)::close(fd);}
};
inline std::string read_file(const std::filesystem::path& path,size_t limit) {
    Descriptor file(::open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NONBLOCK));
    require(file.fd>=0,"cannot open input");
    struct stat info{};
    require(::fstat(file.fd,&info)==0&&S_ISREG(info.st_mode)&&info.st_size>0&&
            uint64_t(info.st_size)<=limit,"input must be a bounded regular file");
    std::string result(size_t(info.st_size),'\0');size_t offset=0;
    while(offset<result.size()) {
        const auto count=::read(file.fd,result.data()+offset,result.size()-offset);
        if(count<0&&errno==EINTR)continue;
        require(count>0,"input read failed or changed");offset+=size_t(count);
    }
    char extra;require(::read(file.fd,&extra,1)==0,"input grew during read");
    return result;
}
inline std::string string_value(Element value) {return std::string(value.get_string().value());}
inline bool has(Element value,const std::string& name) {
    Element found;return !value.at_key(name).get(found);
}
inline void check_tree(Element value,size_t& nodes,size_t depth=0) {
    require(++nodes<=500000&&depth<=64,"JSON node/depth budget exceeded");
    if(value.is_object()) {
        std::set<std::string> keys;
        for(auto field:value.get_object().value()) {
            require(keys.insert(std::string(field.key)).second,"duplicate JSON key");
            check_tree(field.value,nodes,depth+1);
        }
    } else if(value.is_array())for(auto item:value.get_array().value())check_tree(item,nodes,depth+1);
    else if(value.is_number())require(std::isfinite(value.get_double().value()),"nonfinite number");
}
inline void fields(Element value,std::initializer_list<std::string_view> allowed) {
    require(value.is_object(),"expected object");
    for(auto item:value.get_object().value())
        require(std::find(allowed.begin(),allowed.end(),item.key)!=allowed.end(),"unknown proposal field: "+std::string(item.key));
}
inline double number(Element value,double low,double high) {
    require(value.is_number(),"expected numeric value");const double n=value.get_double().value();
    require(std::isfinite(n)&&n>=low&&n<=high,"numeric value out of bounds");return n;
}
inline void vector(Element value,double low,double high) {
    require(value.is_array()&&value.get_array().value().size()==3,"expected three-component vector");
    for(auto coordinate:value.get_array().value())number(coordinate,low,high);
}
inline void field_value(const FieldRule& rule,Element value) {
    if(rule.vector)vector(value,rule.low,rule.high);else number(value,rule.low,rule.high);
}
struct Replacement {
    std::string group,id,key,json;
    int64_t frame=-1;
};
inline Element identified(Element scene,const std::string& group,const std::string& id) {
    for(auto item:scene[group].get_array().value())if(string_value(item["id"])==id)return item;
    throw std::runtime_error("SCENE_EDIT: unknown ID: "+id);
}
inline Element at_frame(Element object,int64_t frame) {
    require(has(object,"keyframes"),"camera has no authored keyframes");
    Element found;size_t count=0;
    for(auto item:object["keyframes"].get_array().value())
        if(item["frame"].get_int64().value()==frame){found=item;++count;}
    require(count==1,"camera frame missing or ambiguous");return found;
}
inline void validate_value(const std::string& group,const std::string& key,Element value,Element object,int64_t frame) {
    const auto& rule=field_rule(key,target_rule(group,true).mask);
    field_value(rule,value);
    if(group=="objects") {
        const auto kind=string_value(object["kind"]);unsigned mask=0;
        for(size_t index=0;index<object_kinds.size();++index)if(object_kinds[index]==kind)mask=1u<<index;
        require((rule.object_kinds&mask)!=0,"unsupported object transform field/kind");
    } else if(group=="cameras") {
        if(rule.pose) {
            require(frame>=0||!has(object,"keyframes")||object["keyframes"].get_array().value().size()==0,
                    "animated camera pose requires an explicit existing frame");
        } else require(frame<0,"frame edits only support camera pose");
    } else {
        require(!has(object,"textures"),"texture-driven material field would be inert; edit an untextured material");
    }
}
inline std::vector<Replacement> proposal_edits(Element scene,Element proposal) {
    const auto version=string_value(proposal["schema"]);
    require(version==edit_schema||version==instance_schema,"unsupported edit schema");
    if(version==edit_schema)fields(proposal,{"schema","edits"});
    else fields(proposal,{"schema","edits","add_instances"});
    auto edits=proposal["edits"].get_array().value();
    require((edits.size()>0||version==instance_schema)&&edits.size()<=max_edits,"edit count outside budget");
    std::vector<Replacement> replacements;std::set<std::tuple<std::string,std::string,int64_t,std::string>> unique;
    for(auto edit:edits) {
        fields(edit,{"target","id","frame","set"});
        const auto target=string_value(edit["target"]),id=string_value(edit["id"]);
        require(!id.empty()&&id.size()<=max_id_length,"invalid identifier length");
        const auto& target_spec=target_rule(target);const std::string group(target_spec.group);
        Element object=identified(scene,group,id),destination=object;
        int64_t frame=-1;
        if(has(edit,"frame")) {
            require(target_spec.frames,"only camera edits accept frame");
            frame=edit["frame"].get_int64().value();require(frame>=first_frame&&frame<=last_frame,"invalid frame");
            destination=at_frame(object,frame);
        }
        auto values=edit["set"].get_object().value();require(values.size()>0&&values.size()<=max_set_fields,"set field count outside budget");
        for(auto item:values) {
            const std::string key(item.key);
            require(has(destination,key),"edit field absent from canonical target");
            require(unique.emplace(group,id,frame,key).second,"overlapping field edits");
            validate_value(group,key,item.value,object,frame);
            replacements.push_back({group,id,key,simdjson::minify(item.value),frame});
        }
    }
    return replacements;
}
inline bool bounded_identifier(const std::string& id) {
    return !id.empty()&&id.size()<=max_id_length&&std::none_of(id.begin(),id.end(),[](unsigned char c){return c<32||c==127;});
}
inline std::vector<std::string> proposal_instances(Element scene,Element proposal) {
    if(string_value(proposal["schema"])==edit_schema)return {};
    auto additions=proposal["add_instances"].get_array().value();
    const auto objects=scene["objects"].get_array().value();
    require(additions.size()<=max_instances&&objects.size()+additions.size()<=max_objects,"instance/object budget exceeded");
    std::set<std::string> ids,assets;
    for(const char* group:{"objects","cameras","lights"})if(has(scene,group))
        for(auto object:scene[group].get_array().value())ids.insert(string_value(object["id"]));
    auto catalog=scene["assets"].get_array().value();require(catalog.size()<=max_objects,"asset catalog budget exceeded");
    for(auto asset:catalog) {
        const auto id=string_value(asset["id"]);
        require(bounded_identifier(id)&&assets.insert(id).second,"invalid or ambiguous catalog asset ID");
    }
    std::vector<std::string> result;
    for(auto spec:additions) {
        fields(spec,{"id","asset","location","yaw_degrees"});
        const auto id=string_value(spec["id"]),asset=string_value(spec["asset"]);
        require(bounded_identifier(id)&&ids.insert(id).second,"invalid or colliding instance ID");
        require(bounded_identifier(asset)&&assets.contains(asset),"unknown catalog asset reference");
        for(const char* field:{"location","yaw_degrees"})field_value(field_rule(field,1),spec[field]);
        result.push_back(asset_instance_json(id,asset,simdjson::minify(spec["location"]),simdjson::minify(spec["yaw_degrees"])));
    }
    return result;
}
inline std::string edited_object(Element value,const std::string& group,const std::string& id,
                                 const std::vector<Replacement>& edits,int64_t frame=-1) {
    std::string result="{";bool first=true;
    for(auto field:value.get_object().value()) {
        const std::string key(field.key);std::string encoded;
        for(const auto& edit:edits)if(edit.group==group&&edit.id==id&&edit.frame==frame&&edit.key==key)encoded=edit.json;
        if(encoded.empty()&&group=="cameras"&&key=="keyframes"&&frame<0) {
            encoded="[";bool first_key=true;
            for(auto keyframe:field.value.get_array().value()) {
                if(!first_key)encoded+=',';first_key=false;
                encoded+=edited_object(keyframe,group,id,edits,keyframe["frame"].get_int64().value());
            }
            encoded+=']';
        }
        if(encoded.empty())encoded=simdjson::minify(field.value);
        if(!first)result+=',';first=false;result+=quote(key)+":"+encoded;
    }
    return result+'}';
}
inline std::string serialize(Element scene,const std::vector<Replacement>& edits,const std::vector<std::string>& additions) {
    std::string result="{";bool first=true;
    for(auto field:scene.get_object().value()) {
        const std::string key(field.key);std::string encoded;
        if(key=="objects"||key=="cameras"||key=="materials") {
            encoded="[";bool first_item=true;
            for(auto item:field.value.get_array().value()) {
                if(!first_item)encoded+=',';first_item=false;
                encoded+=edited_object(item,key,string_value(item["id"]),edits);
            }
            if(key=="objects")for(const auto& addition:additions) {
                if(!first_item)encoded+=',';first_item=false;encoded+=addition;
            }
            encoded+=']';
        } else encoded=simdjson::minify(field.value);
        if(!first)result+=',';first=false;result+=quote(key)+":"+encoded;
        require(result.size()<=scene_limit+proposal_limit,"serialized scene exceeds budget");
    }
    return result+"}\n";
}
inline void valid_pose(Element pose) {
    for(const auto& field:field_rules)if((field.targets&2)&&field.pose)
        vector(pose[std::string(field.name)],field.low,field.high);
    double squared=0;
    for(size_t i=0;i<3;++i) {
        const double delta=pose["location"].at(i).get_double().value()-pose["target"].at(i).get_double().value();
        squared+=delta*delta;
    }
    require(squared>=0.0001&&squared<=100000000,"camera focus distance outside production bounds");
}
inline void exclusive_write(const std::filesystem::path& path,const std::string& contents) {
    Descriptor file(::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600));
    require(file.fd>=0,"output must be a new file in an existing operator directory");
    try {
        size_t offset=0;
        while(offset<contents.size()) {
            const auto count=::write(file.fd,contents.data()+offset,contents.size()-offset);
            if(count<0&&errno==EINTR)continue;
            require(count>0,"output write failed");offset+=size_t(count);
        }
        require(::fsync(file.fd)==0,"output sync failed");
        const int fd=file.fd;file.fd=-1;require(::close(fd)==0,"output close failed");
    } catch(...) {::unlink(path.c_str());throw;}
}
inline void run(const std::filesystem::path& base,const std::filesystem::path& proposal,const std::filesystem::path& output) {
    const auto source=read_file(base,scene_limit),instructions=read_file(proposal,proposal_limit);
    simdjson::dom::parser scene_parser(scene_limit),edit_parser(proposal_limit);
    require(!scene_parser.allocate(source.size(),64)&&!edit_parser.allocate(instructions.size(),64),"parser allocation failed");
    Element scene=scene_parser.parse(source).value(),edit=edit_parser.parse(instructions).value();
    size_t nodes=0;check_tree(scene,nodes);nodes=0;check_tree(edit,nodes);
    require(string_value(scene["schema"])=="singular.scene/1","unsupported canonical scene schema");
    for(const char* group:{"objects","cameras","materials"}) {
        auto array=scene[group].get_array().value();require(array.size()<=max_objects,"canonical collection budget exceeded");
        std::set<std::string> ids;
        for(auto item:array)require(ids.insert(string_value(item["id"])).second,"ambiguous canonical ID");
    }
    auto replacements=proposal_edits(scene,edit);
    const auto additions=proposal_instances(scene,edit);
    require(!replacements.empty()||!additions.empty(),"empty proposal");
    const auto rendered=serialize(scene,replacements,additions);
    if(string_value(edit["schema"])==instance_schema)require(rendered.size()<=scene_limit,"instance result exceeds reusable scene size budget");
    simdjson::dom::parser result_parser(scene_limit+proposal_limit);
    require(!result_parser.allocate(rendered.size(),64),"result parser allocation failed");
    Element result=result_parser.parse(rendered).value();
    if(string_value(edit["schema"])==instance_schema){nodes=0;check_tree(result,nodes);}
    for(auto camera:result["cameras"].get_array().value()) {
        valid_pose(camera);
        if(has(camera,"keyframes"))for(auto keyframe:camera["keyframes"].get_array().value())valid_pose(keyframe);
    }
    exclusive_write(output,rendered);
    std::cout<<"SCENE_EDITED fields="<<replacements.size()<<" added_instances="<<additions.size()<<" output="<<output<<"\n";
}
} // namespace editing
} // namespace singular::scene
