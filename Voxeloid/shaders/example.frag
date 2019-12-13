// https://www.shadertoy.com/view/4sVfWw

uniform float iTime;
uniform vec3 iResolution;

#define detail 5
#define steps 300
#define time iTime*0.5
#define maxdistance 30.0


//ray-cube intersection, on the inside of the cube
vec3 voxel(vec3 lro, vec3 rd, vec3 ird, float size)
{
//  vec3 hit = voxel(lro, rd, ird, size);

    size *= 0.5;
    
    vec3 hit = -(sign(rd)*(lro-size)-size)*ird;
    
    return hit;
}

void mainImage( out vec4 fragColor,  vec2 fragCoord )
{
    
    fragColor = vec4(0.0);
    vec2 uv = (fragCoord.xy * 2.0 - iResolution.xy) /iResolution.y;
	// voxel size
    float size = 1.0;
 
	// ray origin
    vec3 ro = vec3(0.5+sin(time)*0.4,0.5+cos(time)*0.4,time);
	// ray dir
    vec3 rd = normalize(vec3(uv,1.0));
	// local ray origin?
    vec3 lro = mod(ro,size);
	// ray origin moved to voxel corner?
    vec3 fro = ro-lro;
	// ???
    vec3 ird = 1.0/max(abs(rd),0.001);
	// raymarch mask
    vec3 mask;
	// should go upp a level?
    bool exitoct = false;
	// current depth?
    int recursions = 0;
	// ???
    float dist = 0.0;
    float edge = 1.0;
    vec3 lastmask;
    
    //the octree traverser loop
    //each iteration i either:
    // - check if i need to go up a level
    // - check if i need to go down a level
    // - check if i hit a cube
    // - go one step forward if octree cell is empty
    // - repeat if i did not hit a cube
    for (int i = 0; i < steps; i++)
    {
        if (dist > maxdistance) break;
        
        //i go up a level
        if (exitoct)
        {
            
            vec3 newfro = floor(fro/(size*2.0))*(size*2.0);
            
            lro += fro-newfro;
            fro = newfro;
            
            recursions--;
            size *= 2.0;
            
            exitoct = (recursions > 0) && (abs(dot(mod(fro/size+0.5,2.0)-1.0+mask*sign(rd)*0.5,mask))<0.1);
        }
        else
        {

			// 2 = leaf
            //checking what type of cell it is: empty, full or subdivide
            int voxelstate;//= getvoxel(fro,size);
            if (voxelstate == 1 && recursions > detail)
            {
                voxelstate = 0;
            }
            
            if(voxelstate == 1&&recursions<=detail)
            {
				// is node with children

                //if(recursions>detail) break;

                recursions++;
                size *= 0.5;

                //find which of the 8 voxels i will enter
                vec3 mask2 = step(vec3(size),lro);
                fro += mask2*size;
                lro -= mask2*size;
            }
            else if (voxelstate == 0||voxelstate == 2)
            {
				// empty, move forward

                //raycast and find distance to nearest voxel surface in ray direction
                //i don't need to use voxel() every time, but i do anyway
                vec3 hit = voxel(lro, rd, ird, size);
                
                mask = vec3(lessThan(hit,min(hit.yzx,hit.zxy)));
                float len = dot(hit,mask);
                
                if (voxelstate == 2) {
					// is leaf, break
                    break;
                }

                //moving forward in ray direction, and checking if i need to go up a level
                dist += len;
                lro += rd*len-mask*sign(rd)*size;
                vec3 newfro = fro+mask*sign(rd)*size;
                exitoct = (floor(newfro/size*0.5+0.25)!=floor(fro/size*0.5+0.25))&&(recursions>0);
                fro = newfro;
                lastmask = mask;
            }
        }
    }
    ro += rd*dist;

    if(i < steps && dist < maxdistance)
    {
    	float val = fract(dot(fro,vec3(15.23,754.345,3.454)));
        vec3 color = sin(val*vec3(39.896,57.3225,48.25))*0.5+0.5;
    	fragColor = vec4(color*(normal*0.25+0.75),1.0);
        
#ifdef borders
        vec3 q = abs(lro/size-0.5)*(1.0-lastmask);
        edge = clamp(-(max(max(q.x,q.y),q.z)-0.5)*20.0*size,0.0,edge);
#endif
#ifdef blackborders
        fragColor *= edge;
#else
        fragColor = 1.0-(1.0-fragColor)*edge;
#endif
    } else {
        #ifdef blackborders
                fragColor = vec4(edge);
        #else
                fragColor = vec4(1.0-edge);
        #endif
    }
    //fragColor = sqrt(fragColor);
    //fragColor *= 1.0-dist/maxdistance;
}