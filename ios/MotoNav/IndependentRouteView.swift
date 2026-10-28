import SwiftUI
import CoreLocation

enum IndependentMode: String, CaseIterable, Identifiable {
    case motorcycle="摩托车", bicycle="自行车"
    var id: String { rawValue }
}

final class IndependentPlanner: ObservableObject {
    @Published var start: MotorPoint?
    @Published var end: MotorPoint?
    @Published var vias: [MotorPoint]=[]
    @Published var routes: [MotorRoute]=[]
    @Published var selected=0
    @Published var busy=false
    @Published var message=""
    private var generation=UUID()
    private var task: Task<Void,Never>?
    func invalidate() { generation=UUID();task?.cancel();task=nil;routes=[];selected=0;busy=false }
    func plan(_ mode: IndependentMode) {
        guard !WebMotorSession.shared.running,!NaviBridge.shared.navigating,!NaviBridge.shared.cruising else { message="请先结束当前导航或巡航";return }
        guard !Settings.shared.keysMissing else { message="请先在地图 Key 页面填写自己的高德 iOS Key";return }
        let origin: MotorPoint
        if let start=start { origin=start }
        else {
            guard let fix=LocationService.shared.location,fix.horizontalAccuracy>=0,fix.horizontalAccuracy<=100,abs(fix.timestamp.timeIntervalSinceNow)<15 else { message="当前位置尚未就绪，请等待定位或手动选择起点";LocationService.shared.start();return }
            let g=MapGeo.wgsToGcj(fix.coordinate.latitude,fix.coordinate.longitude)
            origin=MotorPoint(lat:g.lat,lng:g.lng,name:"我的位置")
        }
        guard let end=end else { message="请选择目的地";return }
        invalidate();let token=generation
        let request=MotorRequest(start:origin,end:end,vias:mode == .bicycle ? [] : vias,bicycle:mode == .bicycle)
        busy=true;message="正在规划\(mode.rawValue)路线…"
        task=Task { @MainActor [weak self] in
            do {
                let result=try await MotorRoutes.load(request)
                guard let self=self,self.generation==token,!Task.isCancelled else { return }
                self.routes=result;self.selected=0;self.busy=false
                self.message="已找到 \(result.count) 条路线，请选择方案"
            } catch {
                guard let self=self,self.generation==token,!Task.isCancelled else { return }
                self.busy=false;self.message=error.localizedDescription
            }
        }
    }
}

struct PlacePickerView: View {
    let title: String
    let choose: (MotorPoint)->Void
    @StateObject private var search=SearchService()
    @State private var query=""
    @Environment(\.dismiss) private var dismiss
    var body: some View {
        NavigationView {
            List {
                HStack {
                    TextField("搜索地点或地址",text:$query).submitLabel(.search).onSubmit{search.search(query)}
                    GlassChip(title: "搜索", icon: "magnifyingglass", tint: ContentView.pink, foreground: ContentView.pink, compact: true){search.search(query)}
                }
                if !search.status.isEmpty { Text(search.status).font(.caption) }
                ForEach(search.results) { p in
                    Button {
                        choose(MotorPoint(lat:p.lat,lng:p.lng,name:p.name));dismiss()
                    } label: {
                        VStack(alignment:.leading,spacing:4){Text(p.name);Text(p.address).font(.caption).foregroundColor(.secondary)}
                    }
                }
                if search.results.isEmpty && query.isEmpty {
                    ForEach(History.shared.places) { p in
                        Button(p.name){choose(MotorPoint(lat:p.lat,lng:p.lng,name:p.name));dismiss()}
                    }
                }
            }.navigationTitle(title).toolbar{ToolbarItem(placement:.cancellationAction){Button("取消"){dismiss()}}}
        }
    }
}

struct MotorLoginView: View {
    @StateObject private var browser=MotorBrowser()
    let page: URL?
    @Environment(\.dismiss) private var dismiss
    var body: some View {
        NavigationView {
            VStack {
                Text("仅在登录或验证时打开网页。登录保存在本机，正常规划可直接在 App 完成。").font(.caption).padding()
                MotorBrowserView(browser:browser)
                Text(browser.status).font(.caption)
            }.navigationTitle("高德登录／验证").toolbar{ToolbarItem(placement:.confirmationAction){Button("完成"){dismiss()}}}
        }.onAppear{if let page=page {browser.web.load(URLRequest(url:page))}}
    }
}

struct IndependentPlannerView: View {
    @StateObject private var model=IndependentPlanner()
    @ObservedObject var guide=WebMotorSession.shared
    @State private var mode: IndependentMode
    @State private var pick: Int? // 0 origin, 1 destination, 2 via
    @State private var login=false
    @State private var loaded=false
    @Environment(\.dismiss) private var dismiss
    init(mode: IndependentMode) { _mode=State(initialValue:mode) }
    var body: some View {
        NavigationView {
            ScrollView {
                VStack(alignment:.leading,spacing:16) {
                    GlassSegmented(options: IndependentMode.allCases.map { ($0, $0.rawValue) }, selection: $mode)
                        .onChange(of:mode){_ in model.invalidate();model.message=""}
                    CardView {
                        // 起终点选择行：透明玻璃条（控制层浮在卡片上）
                        Button{pick=0}label:{
                            Label(model.start?.name.isEmpty == false ? model.start!.name : "我的位置（点此更改起点）",systemImage:"location")
                                .font(.subheadline.weight(.semibold)).foregroundColor(AppTheme.text)
                                .frame(maxWidth:.infinity,alignment:.leading)
                                .padding(.horizontal,14).padding(.vertical,11)
                                .liquidGlass(Capsule(), tint: AppTheme.cyan.opacity(0.16), interactive: true)
                                .contentShape(Capsule())
                        }.buttonStyle(.plain)
                        if model.start != nil {GlassChip(title: "使用当前位置", icon: "location.fill", foreground: ContentView.sub, compact: true){model.start=nil;model.invalidate()}}
                        Divider()
                        Button{pick=1}label:{
                            Label(model.end?.name.isEmpty == false ? model.end!.name : "输入目的地",systemImage:"mappin.and.ellipse")
                                .font(.subheadline.weight(.semibold)).foregroundColor(AppTheme.text)
                                .frame(maxWidth:.infinity,alignment:.leading)
                                .padding(.horizontal,14).padding(.vertical,11)
                                .liquidGlass(Capsule(), tint: AppTheme.pink.opacity(0.16), interactive: true)
                                .contentShape(Capsule())
                        }.buttonStyle(.plain)
                        if mode == .motorcycle {
                            ForEach(model.vias.indices,id:\.self){i in HStack{Text("途经 \(i+1)：\(model.vias[i].name)").font(.caption).foregroundColor(ContentView.sub);Spacer();GlassChip(title: "移除", icon: "trash", tint: AppTheme.red, foreground: AppTheme.red, compact: true){model.vias.remove(at:i);model.invalidate()}}}
                            if model.vias.count<3 {GlassChip(title: "添加途经点", icon: "plus", tint: ContentView.cyan, foreground: ContentView.cyan, compact: true){pick=2}}
                        }
                        GlassButton(title: model.busy ? "正在规划…" : "规划\(mode.rawValue)路线", icon: "arrow.triangle.swap", style: .prominent, tint: ContentView.pink, enabled: !model.busy && model.end != nil) { model.plan(mode) }
                    }
                    if !model.message.isEmpty {Text(model.message).font(.footnote).foregroundColor(ContentView.sub)}
                    if mode == .motorcycle {GlassChip(title: "高德登录／验证", icon: "person.badge.key", tint: ContentView.cyan, compact: true){login=true}}
                    if let route=model.routes.indices.contains(model.selected) ? model.routes[model.selected] : nil {
                        MotorMap(route:route,position:nil,overview:true).frame(height:260).clipShape(RoundedRectangle(cornerRadius:18))
                        ForEach(Array(model.routes.enumerated()),id:\.offset){i,r in
                            Button{model.selected=i}label:{
                                HStack{
                                    Image(systemName:model.selected==i ? "checkmark.circle.fill" : "circle")
                                    VStack(alignment:.leading,spacing:4){Text("方案 \(i+1) · \(NaviBridge.durText(r.seconds))").bold();Text(NaviBridge.distText(r.totalMeters)).font(.subheadline)}
                                    Spacer()
                                }.padding(14).background(RoundedRectangle(cornerRadius:14).fill(ContentView.card))
                            }
                        }
                        HStack(spacing: 12) {
                            GlassButton(title:"开始导航", icon: "location.fill", style: .prominent, tint: ContentView.pink, enabled: !model.busy){begin(route,false)}
                            GlassButton(title:"模拟导航", icon: "play.fill", style: .tinted, tint: .purple, enabled: !model.busy){begin(route,true)}
                        }
                    }
                    Text(mode == .motorcycle ? "路线由高德摩托车网页服务规划；登录或验证过期时按上方入口处理。" : "使用高德 iOS SDK 自行车规划，路线不会交给驾车导航引擎。").font(.caption).foregroundColor(ContentView.sub)
                }.padding(16)
            }            .background(ContentView.bg).navigationTitle("\(mode.rawValue)路线")
                .toolbar{ToolbarItem(placement:.cancellationAction){Button("返回首页"){dismiss()}}}
        }.foregroundColor(AppTheme.text).tint(ContentView.pink)
        .sheet(isPresented:Binding(get:{pick != nil},set:{if !$0{pick=nil}})){
            PlacePickerView(title:pick==0 ? "选择起点" : pick==2 ? "选择途经点" : "选择目的地"){point in
                if pick==0{model.start=point}else if pick==2{model.vias.append(point)}else{model.end=point}
                model.invalidate();pick=nil
            }
        }
        .sheet(isPresented:$login){MotorLoginView(page:nil)}
        .onAppear{
            guard !loaded else{return};loaded=true
            if let route=guide.route,route.request.bicycle == (mode == .bicycle),!guide.running {
                model.start=route.request.start;model.end=route.request.end;model.vias=route.request.vias;model.routes=[route]
            }
        }
    }
    private func begin(_ route:MotorRoute,_ simulate:Bool){
        guard !NaviBridge.shared.navigating,!NaviBridge.shared.cruising,!guide.running else{model.message="请先结束当前导航或巡航";return}
        History.shared.add(SearchService.Place(name:route.request.end.name,address:"",lat:route.request.end.lat,lng:route.request.end.lng))
        guide.start(route,simulate:simulate)
    }
}

struct IndependentNavigationView: View {
    @ObservedObject var guide=WebMotorSession.shared
    @ObservedObject var settings=Settings.shared
    @ObservedObject var metadata=WebRouteMetadata.shared
    @Environment(\.dismiss) private var dismiss
    @State private var confirmExit=false
    var body: some View {
        ZStack(alignment:.top){
            if let route=guide.route {MotorMap(route:route,position:guide.position,overview:guide.overview).ignoresSafeArea()}
            VStack(spacing:12){
                HStack(alignment:.center,spacing:18){
                    Image(systemName:arrow).font(.system(size:48,weight:.bold)).foregroundColor(ContentView.pink).frame(width:58)
                    VStack(alignment:.leading,spacing:6){
                        Text(NaviBridge.distText(guide.turnMetres)).font(.system(size:30,weight:.heavy))
                        Text(guide.instruction).font(.headline).lineLimit(2)
                        Text(guide.status).font(.caption).foregroundColor(ContentView.sub)
                    };Spacer(minLength:0)
                }.padding(18).liquidGlass(RoundedRectangle(cornerRadius: 24))
                Spacer()
                VStack(spacing:12){
                    HStack{
                        Text("\(guide.speed)").font(.system(size:32,weight:.bold))+Text(" km/h").font(.caption)
                        Spacer()
                        VStack(alignment:.trailing){Text("剩余 \(NaviBridge.distText(guide.remaining))").bold();Text("约 \(NaviBridge.durText(guide.remainingSeconds))").font(.caption)}
                    }
                    if !metadata.status.isEmpty {Text(metadata.status).font(.caption2).foregroundColor(ContentView.sub)}
                    HStack(spacing:10){
                        GlassChip(title: guide.overview ? "回到跟随" : "看全程", icon: guide.overview ? "location.fill" : "map", tint: ContentView.cyan, foreground: ContentView.cyan, compact: true){guide.overview.toggle()}
                        Spacer()
                        GlassChip(title: settings.voiceMuted ? "取消静音" : "静音", icon: settings.voiceMuted ? "speaker.slash.fill" : "speaker.wave.2.fill", tint: settings.voiceMuted ? AppTheme.pink : nil, foreground: settings.voiceMuted ? AppTheme.pink : nil, compact: true){settings.voiceMuted.toggle();if settings.voiceMuted{Speech.shared.stopNow()}}
                        Spacer()
                        GlassChip(title: "返回设置", icon: "gearshape", compact: true){dismiss()}
                    }
                    if guide.simulation {
                        HStack(spacing:10){
                            GlassChip(title: guide.paused ? "继续" : "暂停", icon: guide.paused ? "play.fill" : "pause.fill", tint: AppTheme.pink, foreground: AppTheme.pink, compact: true){guide.paused.toggle()}
                            Spacer()
                            GlassChip(title: "\(guide.speed) km/h", icon: "speedometer", compact: true){guide.speed=guide.speed==60 ? 120 : guide.speed==120 ? 180 : 60}
                        }
                    }
                    GlassButton(title:"结束导航", icon: "stop.fill", style: .destructive){confirmExit=true}
                }.padding(18).liquidGlass(RoundedRectangle(cornerRadius: 24))
            }.padding(16)
            if guide.arrived {
                VStack(spacing:18){
                    Text("已到达目的地").font(.title.bold()).foregroundColor(AppTheme.text)
                    GlassChip(title: "返回首页", icon: "house.fill", tint: ContentView.pink, foreground: ContentView.pink){guide.arrived=false;dismiss()}
                }
                .padding(30).liquidGlass(RoundedRectangle(cornerRadius: 24)).frame(maxHeight:.infinity)
            }
        }.foregroundColor(AppTheme.text).tint(ContentView.pink)
        .alert("结束导航？",isPresented:$confirmExit){Button("结束",role:.destructive){guide.stop();guide.arrived=false;dismiss()};Button("继续导航",role:.cancel){}}
    }
    private var arrow:String {
        switch guide.turnDirection {case "L":return "arrow.turn.up.left";case "R":return "arrow.turn.up.right";case "SL":return "arrow.up.left";case "SR":return "arrow.up.right";case "U":return "arrow.uturn.down";default:return "arrow.up"}
    }
}
