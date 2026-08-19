const C=require('../../ime/core/cheonjiin.js');
let o=[];
for(const c of C.CHO)for(const v of C.JUNG)for(const j of C.JONG){
  const ch=C.compose(c,v,j.trim());
  o.push(ch+'\t'+C.encode(ch,false).join(' '));
}
process.stdout.write(o.join('\n')+'\n');
