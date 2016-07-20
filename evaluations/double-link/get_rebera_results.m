function res = get_rebera_results(cap,scenario,path)
% GET_REBERA_RESULTS    get rebera performance metrics for each scenario
% under a given path
% res = get_rebera_results(cap,scenario,path)
% cap: capacity time series, kbits per each second
% scenario: under investigation
% path: folder path, may contain multiple experiment folders
% res: we gather 3 statistics; bw util % and 95perc packet and frame delays
% =========================================================================

    contents = (dir(path))';
    
    res = zeros( sum([contents.isdir])-2, 3 );
    
    emul_no = 1;
    for emul = contents
        if emul.isdir && ~(strcmp(emul.name,'.') || strcmp(emul.name,'..'))
            emul_id = emul.name;
            emul_path = [path,emul_id,'/'];
            
            if ~exist([emul_path,'packets.mat'],'file')
                
                % read packet stats: [sending_clock_time, seq_no, length_bytes, frame_no ]
                reberapac = dlmread([emul_path,'sender_packets.txt']);
                
                % use relative timestamps
                reberapac(:,1) = reberapac(:,1)-dlmread([emul_path,'clock']);
                
                % read ack packets: [1way_delay_us, seq_no, frame_no]
                ack = dlmread([emul_path,'ack.txt']);
                
                % analyze only acked packets
                max_seq = min( max(ack(:,2)), max(reberapac(:,2)) );
                reberapac( reberapac(:,2)>max_seq, : ) = []; % trimming
                ack( ack(:,2)>max_seq, : ) = []; % trimming
                
                % find the sent packets that have been acked; +1 column
                reberapac = equ_rebera( ack, reberapac );
                
                save([emul_path,'packets.mat'], 'reberapac');
            else
                load([emul_path,'packets.mat']);
            end
            
            % generate sending rate time series, kbits per each second
            rate = gen_rate( reberapac(:,1), reberapac(:,3)*8e-3, 1e6);
            maxlen = min( [length(rate), length(cap)] );
            
            % plot the empirical CDF of the 1-way delays (ms)
            ecdf(reberapac(:,5)/1e3)
            
            res(emul_no,1) = 100*sum(rate(1:maxlen))/sum(cap(1:maxlen));
            res(emul_no,2) = prctile(reberapac(:,5),95)/1000;
%             res(emul_no,2) = mean(reberapac(:,5))/1000;
            res(emul_no,3) = prctile(frmdly( reberapac(:,[5 2 4]) ),95)/1000;
%             res(emul_no,3) = mean(frmdly( reberapac(:,[5 2 4]) ))/1000;
            
            if 0
                subplot(2,1,1)
                plot(rate(1:maxlen),'r');
                subplot(2,1,2)
                plot(reberapac(:,1)/1e6,reberapac(:,5)/1e3)
            end

            display([scenario,' => FaceTime bw utilization=',num2str(res(emul_no,1)),' %'])
            display([scenario,' => Rebera 95% packet delay=',num2str(res(emul_no,2))])
            display([scenario,' => Rebera 95% frame delay=',num2str(res(emul_no,3))])
            
            emul_no = emul_no+1;
        end
    end
return

function intsec = equ_rebera( ack_, pac_ )
% EQU_REBERA    finds the sent Rebera packets that have been acked
% ack_: acked packets
% pac_: sent packets
% intsec: intersection packet stats 
% [sending_clock_time, seq_no, length, frame_no 1way_delay_us]
% ================================================================

% allocate memory for speed
intsec = zeros(size(ack_,1),size(pac_,2)+1);

i_pointer = 1;
% nomatch = zeros( size(ack_,1), 1 ); 
% d_pointer = 1;

for i = 1 : size(ack_,1) % for each acked packet
    
    % find the one in the larger set with the same sequence number
    ix = find( pac_(:,2) == ack_(i,2) );
    
    % key found in the big set => ith element belongs to intersection
    if ~isempty(ix)
        assert(length(ix)==1);
        intsec(i_pointer,:) = [pac_(ix,:),ack_(i,1)];
        i_pointer = i_pointer + 1;
        pac_(ix,:) = [];
    end
end
intsec = intsec(1:i_pointer-1, :);
return