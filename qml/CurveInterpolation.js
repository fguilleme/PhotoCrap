.pragma library

// Keep the editor's existing interpolation arithmetic and endpoint rules.
function prepare(points) {
    if(points.length<2)return []
    const delta=[],slopes=[]
    for(let i=0;i+1<points.length;i++)
        delta.push((points[i+1].y-points[i].y)/Math.max(.001,points[i+1].x-points[i].x))
    slopes.push(delta[0])
    for(let i=1;i+1<points.length;i++) {
        if(delta[i-1]*delta[i]<=0)slopes.push(0)
        else {
            const left=points[i].x-points[i-1].x,right=points[i+1].x-points[i].x
            slopes.push((left+right)/((left+2*right)/(3*delta[i-1])+(2*left+right)/(3*delta[i])))
        }
    }
    slopes.push(delta[delta.length-1])
    return slopes
}

function value(points,slopes,input) {
    if(points.length<2)return input
    if(input<=points[0].x)return points[0].y
    if(input>=points[points.length-1].x)return points[points.length-1].y
    let segment=0
    while(segment+1<points.length-1 && input>points[segment+1].x)segment++
    const width=points[segment+1].x-points[segment].x
    const t=(input-points[segment].x)/width,t2=t*t,t3=t2*t
    return (2*t3-3*t2+1)*points[segment].y+(t3-2*t2+t)*width*slopes[segment]
            +(-2*t3+3*t2)*points[segment+1].y+(t3-t2)*width*slopes[segment+1]
}

function sample(points,slopes) {
    const samples=[]
    for(let i=0;i<256;++i)samples.push(value(points,slopes,i))
    return samples
}
