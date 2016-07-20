function cap = get_capacity( scenario )

    if ~exist(['./',scenario,'/capacity.mat'],'file')
        link = strsplit(scenario,'-');
        if length(link) > 1
            cap_up = gen_rate(['../cleaned_traces/',link{1},'-uplink.tr'],1.442*8,1e3);
            cap_dn = [0;gen_rate(['../cleaned_traces/',link{2},'-downlink.tr'],1.442*8,1e3)];
            maxlen = min( [length(cap_up), length(cap_dn)] );
            cap_up = cap_up(1:maxlen); cap_dn = cap_dn(1:maxlen);
            cap = min(cap_up,cap_dn);
        else
            cap = gen_rate(['../cleaned_traces/',link{1},'-uplink.tr'],1.442*8,1e3);
        end
        save(['./',scenario,'/capacity.mat'],'cap');
    else
        load(['./',scenario,'/capacity.mat']);
    end

return