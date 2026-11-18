const {chromium}=require('playwright');
(async()=>{const b=await chromium.launch();const p=await b.newPage({viewport:{width:1200,height:732},deviceScaleFactor:2});
for(const m of ['virus','sylenth','spire']){await p.goto('file://'+process.cwd()+'/mock_'+m+'.html');await p.screenshot({path:'grid_'+m+'.png'});}
await b.close();})();
