dbstop if error
close all
clc

facetime_res = [];
rebera_res = [];
scenarios = cell(0);

for l = (dir('*'))'
    if l.isdir && ~(strcmp(l.name,'.') || strcmp(l.name,'..'))
        
        scenario = l.name; scenarios = [scenarios; scenario];
        cap = get_capacity( scenario );
        f = figure; box on; 
%         subplot(2,1,1); 
%         plot(cap,'k'); 
        hold all;
        
        for item = (dir(['./',scenario,'/']))'
            if item.isdir && ~(strcmp(item.name,'.') || strcmp(item.name,'..'))

                app = item.name; path = ['./',scenario,'/',app,'/'];
                
                if strcmp(app,'facetime')
                    res = mean(get_facetime_results(cap,scenario,path,f),1);
                    facetime_res = [facetime_res; res];
                elseif strcmp(app,'rebera')
                    res = mean(get_rebera_results(cap,scenario,path,f),1);
                    rebera_res = [rebera_res; res];
                end
            end
        end
    end
end

%% plots
figure; hold all; box on;
scatter(facetime_res(:,1),facetime_res(:,3),20,'b','LineWidth',1,'Marker','.')
labelpoints(facetime_res(:,1), facetime_res(:,3), scenarios, 'C', 'FontSize', 10, 'Color', 'b');
scatter(rebera_res(:,1),rebera_res(:,3),20,'r','LineWidth',1,'Marker','.')
labelpoints(rebera_res(:,1), rebera_res(:,3), scenarios, 'C', 'FontSize', 10, 'Color', 'r');
xlabel('ABW utilization percentage','FontSize',20)
ylabel('95-percentile frame delays (msec)','FontSize',20)
title('Double-link scenarios','FontSize',20)
legend('FaceTime','Rebera','FontSize',20)

%% averages
display(['FaceTime frame delay: ',num2str(mean(facetime_res(:,3))),' msec'])
display(['Rebera frame delay: ',num2str(mean(rebera_res(:,3))),' msec'])
display(['FaceTime util: ',num2str(mean(facetime_res(:,1))),' %'])
display(['Rebera util: ',num2str(mean(rebera_res(:,1))),' %'])