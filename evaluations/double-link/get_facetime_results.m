function res = get_facetime_results(cap,scenario,path)
% GET_FACETIME_RESULTS    get facetime performance metrics for each scenario
% under a given path
% res = get_facetime_results(cap,scenario,path)
% cap: capacity time series, kbits per each second
% scenario: under investigation
% path: folder path, may contain multiple experiment folders
% res: we gather 3 statistics; bw util % and 95perc packet and frame delays
% =========================================================================

    con = (dir(path))';
    
    res = zeros(sum([con.isdir])-2,3);
    
    emul_no = 1;
    for emul = con
        if emul.isdir && ~(strcmp(emul.name,'.') || strcmp(emul.name,'..'))
            emul_id = emul.name;
            emul_path = [path,emul_id,'/'];
            
            if ~exist([emul_path,'packets.mat'],'file')
                links = strsplit(scenario,'-');
                
                if length(links) > 1 % this is a double-link scenario
                    
                    % check wireshark packets captured on each cellsim
                    % emulator
                    pac = cell(1,2); d=1;
                    for direction = {'uplink','downlink'}
                        target = strcat(emul_path,direction,'.txt');
                        assert(exist(target{1},'file')==2);
                        
                        all_ = dlmread(target{1}); % read all packets
                        in_  = all_(all_(:,3)<1,:); in_(:,3)=[]; % incoming packets
                        out_ = all_(all_(:,3)>0,:); out_(:,3)=[]; % outgoing packets
                        
                        % find all incoming packets that went out
                        pac{d} = equ_facetime(out_,in_,-1); d=d+1;
                    end
                    
                    % find all packets that passed through both links
                    pac = equ_facetime(pac{2},pac{1},1);
                    
                else % this is a single-link scenario
                    
                    if exist([emul_path,'uplink.txt'],'file')
                        target = [emul_path,'uplink.txt'];
                    elseif exist([emul_path,'downlink.txt'],'file')
                        target = [emul_path,'downlink.txt'];
                    else
                        continue;
                    end
                    
                    % for the link (emulator) found
                    all_ = dlmread(target); % read all packets
                    in_  = all_(all_(:,3)<1,:); in_(:,3)=[]; % incoming packets
                    out_ = all_(all_(:,3)>0,:); out_(:,3)=[]; % outgoing packets
                    
                    % find all incoming packets that went out
                    pac = equ_facetime(out_,in_,-1);
                end
                
                % finally, find the sent packets with known 1-way delays
                pac = equ_facetime(pac,dlmread([emul_path,'send.txt']),0);
                save([emul_path,'packets.mat'], 'pac');
            else
                load([emul_path,'packets.mat']);
            end
            
             % generate sending rate time series, kbits per each second
            rate = gen_rate(pac(:,1),pac(:,4)*8e-3,1);
            maxlen = min( [length(rate), length(cap)] );
            
            % plot the empirical CDF of the 1-way delays (ms)
            ecdf(pac(:,2)*1000)
            
            res(emul_no,1) = 100*sum(rate(1:maxlen))/sum(cap(1:maxlen));
            res(emul_no,2) = prctile(pac(:,2),95)*1000;
%             res(emul_no,2) = mean(pac(:,2))*1000;
            res(emul_no,3) = prctile(frmdly([pac(:,[2 3 4 5]),pac(:,1)]),95)*1000;
%             res(emul_no,3) = mean(frmdly([pac(:,[2 3 4 5]),pac(:,1)]))*1000;
            
            if 0
                subplot(2,1,1); hold all; 
                title(scenario); ylabel('kbps'); xlabel('sec')
                plot(rate(1:maxlen),'b');
                subplot(2,1,2); hold all; ylabel('msec'); xlabel('sec')
                plot(pac(:,1),pac(:,2)*1000)                
            end

            display([scenario,' => FaceTime bw utilization=',num2str(res(emul_no,1)),' %'])
            display([scenario,' => FaceTime 95% packet delay=',num2str(res(emul_no,2))])
            display([scenario,' => FaceTime 95% frame delay=',num2str(res(emul_no,3))])
            
            emul_no = emul_no+1;
        end
    end
return

function intsec = equ_facetime( small_, big_, op )
% EQU    finds the intersection of 2 lists of packets
% small_: smaller list of packets
% big_: bigger list of packets
% op: operation mode; 0 for adding 1-way delays, +1 for adding up delays in
% different links, -1 for finding the time difference (link queuing delay)
% intsec: intersection packet stats 
% [sending_clock_time, seq_no, length, frame_no 1way_delay_us]
% ================================================================

assert(size(small_,2)==size(big_,2));
assert( op==1 || op==-1 || op==0 );

if op == 0 % add 1 more column to small_
    intsec  = zeros(size(small_,1),size(small_,2)+1);
else % otherwise keep the number of columns
    intsec  = zeros( size(small_) );
end

i_pointer = 0;

for i = 1 : size(small_,1) % for each element of the small set
    
    % find the one(s) in the larger set with the same key
    ix = find( big_(:,2) == small_(i,2) );
    
    if ~isempty(ix) % key found in the big set => ith element belongs to intersection
        j = ix(1);
        if isequal( big_(j,2:end), small_(i,2:end) )
            i_pointer = i_pointer + 1;
            if op == 0
                intsec(i_pointer,:) = [big_(j,1),small_(i,:)];
            else
                intsec(i_pointer,:) = [small_(i,1)+op*big_(j,1),small_(i,2:end)];
            end
            big_(j,:) = []; % works because it's done only for ix(1)
        end
    end
end
intsec = intsec(1:i_pointer, :);
return