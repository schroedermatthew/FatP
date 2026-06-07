function dubins_jet_demo
    % DUBINS_JET_DEMO  Single-segment Dubins shortest path over the MATLAB Jet.
    %
    %   Cross-check that the MATLAB Jet reproduces the C++ / Python Dubins
    %   gradient. The geometry is the same closed form as dubins_geometry.h,
    %   written once over a scalar so it runs with doubles (the path) and with
    %   Jets (length plus exact gradient). For start (0,0,0) -> goal (3,3,pi/2),
    %   rho = 1, the result is word LSL, length pi/2 + 2*sqrt(2), and gradient
    %   dL/d(x1,y1,th1,rho) = (0.70710678, 0.70710678, 0.29289322, 0.15658276).
    %
    %   Run from the folder containing Jet.m:  >> dubins_jet_demo
    %
    %   Note: seeding here is 1-based (Jet.seed(value, k, N), k = 1..N), the
    %   MATLAB convention; the C++ and Python ports are 0-based.

    x0 = 0; y0 = 0; th0 = 0;
    x1 = 3; y1 = 3; th1 = pi / 2;
    rho = 1;

    % --- path with doubles --------------------------------------------------
    r = dubinsShortest(x0, y0, th0, x1, y1, th1, rho);
    fprintf('word = %s   (t,p,q) = (%.4f, %.4f, %.4f)   length = %.6f\n', ...
        r.word, valueOf(r.t), valueOf(r.p), valueOf(r.q), valueOf(r.length));

    % --- length + gradient with Jets (seed x1,y1,th1,rho; start fixed) ------
    jx1  = Jet.seed(x1, 1, 4);
    jy1  = Jet.seed(y1, 2, 4);
    jth1 = Jet.seed(th1, 3, 4);
    jrho = Jet.seed(rho, 4, 4);
    jx0  = Jet.constant(x0, 4);
    jy0  = Jet.constant(y0, 4);
    jth0 = Jet.constant(th0, 4);
    rj = dubinsShortest(jx0, jy0, jth0, jx1, jy1, jth1, jrho);
    g  = rj.length.d;

    labels = {'dL/dx1', 'dL/dy1', 'dL/dth1', 'dL/drho'};
    fprintf('\nactive word matches the double path: %s\n', yesno(strcmp(rj.word, r.word)));
    for i = 1:4
        fprintf('  %-8s = %+.8f\n', labels{i}, g(i));
    end

    % --- compare to the recorded C++ result ---------------------------------
    cppLen  = pi / 2 + 2 * sqrt(2);
    cppGrad = [0.70710678, 0.70710678, 0.29289322, 0.15658276];
    fprintf('\n=== vs recorded C++ gradient ===\n');
    dmax = abs(rj.length.v - cppLen);
    for i = 1:4
        dmax = max(dmax, abs(g(i) - cppGrad(i)));
        fprintf('  %-8s MATLAB=%+.8f  C++=%+.8f  |d|=%.1e\n', labels{i}, g(i), cppGrad(i), abs(g(i) - cppGrad(i)));
    end
    fprintf('  length    MATLAB=%.6f  C++=%.6f\n', rj.length.v, cppLen);
    fprintf('max |MATLAB - C++| = %.2e\n', dmax);

    if dmax < 1e-6
        fprintf('\nCROSS-CHECK PASSED: the MATLAB Jet reproduces the C++ Dubins gradient.\n');
    else
        error('dubins:crosscheck', 'cross-check failed');
    end
end

% ============================================================================
% Geometry (templated on the scalar: double for the path, Jet for the gradient)
% ============================================================================

function v = valueOf(x)
    if isa(x, 'Jet'), v = x.v; else, v = x; end
end

function r = mod2pi(x)
    k = floor(valueOf(x) / (2 * pi));
    r = x - (2 * pi) * k;   % subtract a constant; for a Jet the partials pass through
end

function [ok, t, p, q] = wordLSL(a, b, d)
    ok = false; t = 0; p = 0; q = 0;
    psq = 2 + d * d - 2 * cos(a - b) + 2 * d * (sin(a) - sin(b));
    if valueOf(psq) < 0, return; end
    u = atan2(cos(b) - cos(a), d + sin(a) - sin(b));
    t = mod2pi(-a + u);
    p = sqrt(psq);
    q = mod2pi(b - u);
    ok = true;
end

function [ok, t, p, q] = wordRSR(a, b, d)
    ok = false; t = 0; p = 0; q = 0;
    psq = 2 + d * d - 2 * cos(a - b) + 2 * d * (sin(b) - sin(a));
    if valueOf(psq) < 0, return; end
    u = atan2(cos(a) - cos(b), d - sin(a) + sin(b));
    t = mod2pi(a - u);
    p = sqrt(psq);
    q = mod2pi(-b + u);
    ok = true;
end

function [ok, t, p, q] = wordLSR(a, b, d)
    ok = false; t = 0; p = 0; q = 0;
    psq = -2 + d * d + 2 * cos(a - b) + 2 * d * (sin(a) + sin(b));
    if valueOf(psq) < 0, return; end
    p = sqrt(psq);
    u = atan2(-cos(a) - cos(b), d + sin(a) + sin(b)) - atan2(-2, p);
    t = mod2pi(-a + u);
    q = mod2pi(-b + u);
    ok = true;
end

function [ok, t, p, q] = wordRSL(a, b, d)
    ok = false; t = 0; p = 0; q = 0;
    psq = -2 + d * d + 2 * cos(a - b) - 2 * d * (sin(a) + sin(b));
    if valueOf(psq) < 0, return; end
    p = sqrt(psq);
    u = atan2(cos(a) + cos(b), d - sin(a) - sin(b)) - atan2(2, p);
    t = mod2pi(a - u);
    q = mod2pi(b - u);
    ok = true;
end

function [ok, t, p, q] = wordRLR(a, b, d)
    ok = false; t = 0; p = 0; q = 0;
    tmp = (6 - d * d + 2 * cos(a - b) + 2 * d * (sin(a) - sin(b))) / 8;
    if abs(valueOf(tmp)) > 1, return; end
    p = mod2pi(2 * pi - acos(tmp));
    t = mod2pi(a - atan2(cos(a) - cos(b), d - sin(a) + sin(b)) + mod2pi(p / 2));
    q = mod2pi(a - b - t + mod2pi(p));
    ok = true;
end

function [ok, t, p, q] = wordLRL(a, b, d)
    ok = false; t = 0; p = 0; q = 0;
    tmp = (6 - d * d + 2 * cos(a - b) + 2 * d * (-sin(a) + sin(b))) / 8;
    if abs(valueOf(tmp)) > 1, return; end
    p = mod2pi(2 * pi - acos(tmp));
    t = mod2pi(-a - atan2(cos(a) - cos(b), d + sin(a) - sin(b)) + p / 2);
    q = mod2pi(mod2pi(b) - a - t + mod2pi(p));
    ok = true;
end

function r = dubinsShortest(x0, y0, th0, x1, y1, th1, rho)
    dx = x1 - x0;
    dy = y1 - y0;
    d = hypot(dx, dy) / rho;
    theta = mod2pi(atan2(dy, dx));
    alpha = mod2pi(th0 - theta);
    beta  = mod2pi(th1 - theta);

    names = {'LSL', 'LSR', 'RSL', 'RSR', 'RLR', 'LRL'};
    best = inf;
    r = struct('word', 'NONE', 't', 0, 'p', 0, 'q', 0, 'length', 0);
    for i = 1:6
        switch i
            case 1, [ok, t, p, q] = wordLSL(alpha, beta, d);
            case 2, [ok, t, p, q] = wordLSR(alpha, beta, d);
            case 3, [ok, t, p, q] = wordRSL(alpha, beta, d);
            case 4, [ok, t, p, q] = wordRSR(alpha, beta, d);
            case 5, [ok, t, p, q] = wordRLR(alpha, beta, d);
            case 6, [ok, t, p, q] = wordLRL(alpha, beta, d);
        end
        if ~ok, continue; end
        total = t + p + q;
        lv = valueOf(total);
        if lv < best
            best = lv;
            r.word = names{i};
            r.t = t; r.p = p; r.q = q;
            r.length = total * rho;
        end
    end
end

function s = yesno(b)
    if b, s = 'yes'; else, s = 'NO'; end
end
